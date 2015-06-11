# -*- mode: ruby -*-
# # vi: set ft=ruby :

# NOTE: This monkey-patching of the coreos guest plugin is a terrible
# hack that needs to be removed once the upstream plugin works with
# alpha CoreOS images.

require 'tempfile'
require Vagrant.source_root.join("plugins/guests/coreos/cap/change_host_name.rb")

CLOUD_CONFIG = <<EOF
#cloud-config

hostname: %s
EOF

module VagrantPlugins
  module GuestCoreOS
    module Cap
      class ChangeHostName
        def self.change_host_name(machine, name)
          machine.communicate.tap do |comm|
            temp = Tempfile.new("coreos-vagrant")
            temp.binmode
            temp.write(CLOUD_CONFIG % [name])
            temp.close

            path = "/var/tmp/hostname.yml"
            path_esc = path.gsub("/", "-")[1..-1]
            comm.upload(temp.path, path)
            comm.sudo("systemctl start system-cloudinit@#{path_esc}.service")
          end
        end
      end
    end
  end
end
