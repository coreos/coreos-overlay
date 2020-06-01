# -*- mode: ruby -*-
# # vi: set ft=ruby :

# NOTE: This monkey-patching is done to disable to old cloud config based
# change_host_name built into the upstream vagrant project

require Vagrant.source_root.join("plugins/guests/coreos/cap/change_host_name.rb")

module VagrantPlugins
  module GuestCoreOS
    module Cap
      class ChangeHostName
        def self.change_host_name(machine, name)
        end
      end
    end
  end
end
