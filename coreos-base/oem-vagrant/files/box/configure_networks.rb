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
          public_ipv4, private_ipv4 = get_environment_ips(machine, "127.0.0.1")
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
            path_esc = path.gsub("/", "-")[1..-1]
            comm.upload(temp.path, path)
            comm.sudo("systemctl start system-cloudinit@#{path_esc}.service")
          end
        end

        # Find IP addresses to export in /etc/environment. This only works
        # for static addresses defined in the user's Vagrantfile.
        def self.get_environment_ips(machine, default)
          public_ipv4 = nil
          private_ipv4 = nil

          machine.config.vm.networks.each do |type, options|
            next if !options[:ip]
            if type == :public_network
              public_ipv4 = options[:ip]
            elsif type == :private_network
              private_ipv4 = options[:ip]
            end
          end

          # Fall back to localhost if no static networks are configured.
          private_ipv4 ||= default
          public_ipv4 ||= private_ipv4
          return [public_ipv4, private_ipv4]
        end

        def self.match_by_name(machine)
          match = {}
          machine.communicate.tap do |comm|
            comm.sudo("ifconfig -a | grep '^en\\|^eth' | cut -f1 -d:") do |_, result|
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
