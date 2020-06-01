# -*- mode: ruby -*-
# # vi: set ft=ruby :

# NOTE: This monkey-patching is done to disable to old cloud config based
# configure_networks built into the upstream vagrant project

require Vagrant.source_root.join("plugins/guests/coreos/cap/configure_networks.rb")

module VagrantPlugins
  module GuestCoreOS
    module Cap
      class ConfigureNetworks
        def self.configure_networks(machine, networks)
        end
      end
    end
  end
end
