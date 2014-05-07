# -*- mode: ruby -*-
# # vi: set ft=ruby :

# NOTE: This monkey-patching of the coreos guest plugin is a terrible
# hack that needs to be removed once the upstream plugin works with
# alpha CoreOS images.

require 'tempfile'
require 'ipaddr'
require 'log4r'
require Vagrant.source_root.join("plugins/guests/coreos/cap/configure_networks.rb")

BASE_CLOUD_CONFIG = <<EOF
#cloud-config

write_files:
  - path: /etc/environment
    content: |
      COREOS_PUBLIC_IPV4=%s
      COREOS_PRIVATE_IPV4=%s
coreos:
    units:
EOF

NETWORK_UNIT = <<EOF
      - name: %s
        runtime: no
        content: |
          [Match]
          %s

          [Network]
          Address=%s
EOF

# Borrowed from http://stackoverflow.com/questions/1825928/netmask-to-cidr-in-ruby
IPAddr.class_eval do
  def to_cidr
    self.to_i.to_s(2).count("1")
  end
end

module VagrantPlugins
  module GuestCoreOS
    module Cap
      class ConfigureNetworks
        @@logger = Log4r::Logger.new("vagrant::guest::coreos::configure_networks")

        def self.configure_networks(machine, networks)
          public_ipv4, private_ipv4 = get_environment_ips(machine, networks)
          cfg = BASE_CLOUD_CONFIG % [public_ipv4, private_ipv4]

          # Define network units by mac address if possible.
          match_rules = {}
          if false
            #if machine.provider.capability?(:nic_mac_addresses)
            # untested, required feature hasn't made it into a release yet
            match_rules = match_by_mac(machine)
          else
            match_rules = match_by_name(machine)
          end

          @@logger.debug("Networks: #{networks.inspect}")
          @@logger.debug("Interfaces: #{match_rules.inspect}")

          # Generate any static networks, let DHCP handle the rest
          networks.each do |network|
            next if network[:type].to_sym != :static
            interface = network[:interface].to_i
            unit_name = "50-vagrant%d.network" % [interface]

            match = match_rules[interface]
            if match.nil?
                @@logger.warn("Could not find match rule for network #{network.inspect}")
                next
            end

            cidr = IPAddr.new(network[:netmask]).to_cidr
            address = "%s/%s" % [network[:ip], cidr]
            cfg << NETWORK_UNIT % [unit_name, match, address]
          end

          machine.communicate.tap do |comm|
            temp = Tempfile.new("coreos-vagrant")
            temp.binmode
            temp.write(cfg)
            temp.close

            path = "/var/tmp/networks.yml"
            path_esc = path.gsub("/", "-")
            comm.upload(temp.path, path)
            comm.sudo("systemctl start system-cloudinit@#{path_esc}.service")
          end
        end

        def self.get_ipv4(machine, interface)
          ipv4 = nil
          machine.communicate.tap do |comm|
            comm.sudo("ifconfig #{interface} | grep 'inet ' | sed 's/^ *//' | cut -f 2 -d ' '") do |_, result|
              ipv4 = result.gsub("\n", "")
            end
          end
          ipv4
        end

        # Find IP addresses to export in /etc/environment.
        def self.get_environment_ips(machine, networks)
          default_ipv4 = "127.0.0.1"
          public_ipv4  = nil
          private_ipv4 = nil

          current = 0
          interface_to_adapter = {}
          vm_adapters = machine.provider.driver.read_network_interfaces
          vm_adapters.sort.each do |number, adapter|
            if adapter[:type] != :none
              interface_to_adapter[current] = number
              current += 1
            end
          end

          machine.communicate.tap do |comm|
            interfaces = []
            comm.sudo("ifconfig | grep ^en | cut -f1 -d:") do |_, result|
              interfaces = result.split("\n")
            end

            default_ipv4 = get_ipv4(machine, interfaces[0]) || default_ipv4

            networks.each do |network|
              adapter = vm_adapters[interface_to_adapter[network[:interface]]]
              if adapter[:type] == :hostonly
                if network[:type] == :static && network[:ip]
                  private_ipv4 = network[:ip]
                else
                  private_ipv4 = get_ipv4(machine, interfaces[network[:interface]])
                end
              elsif adapter[:type] == :bridged
                if network[:type] == :static && network[:ip]
                  public_ipv4 = network[:ip]
                else
                  public_ipv4 = get_ipv4(machine, interfaces[network[:interface]])
                end
              end
            end
          end

          # Fall back to localhost if no static networks are configured.
          private_ipv4 ||= default_ipv4
          public_ipv4 ||= private_ipv4
          return [public_ipv4, private_ipv4]
        end

        def self.match_by_name(machine)
          match = {}
          machine.communicate.tap do |comm|
            comm.sudo("ifconfig -a | grep ^en | cut -f1 -d:") do |_, result|
              result.split("\n").each_with_index do |name, interface|
                match[interface] = "Name=#{name}"
              end
            end
          end
          match
        end

        def self.match_by_mac(machine)
          match = {}
          macs = machine.provider.capability(:nic_mac_addresses)
          macs.each do |adapter, address|
            # The adapter list from VirtualBox is 1 indexed instead of 0
            interface = adapter.to_i - 1
            match[interface] = "MACAddress=#{address}"
          end
          match
        end
      end
    end
  end
end
