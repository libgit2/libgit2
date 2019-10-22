# -*- mode: ruby -*-
# vi: set ft=ruby :

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.

def install_rmate(machine)
  machine.vm.provision "file", source: "~/bin/rmate", destination: "/tmp/rmate"

  machine.vm.provision "shell", inline: <<-SHELL
    if [ ! -x /usr/local/bin/rmate ]; then
      sudo cp /tmp/rmate /usr/local/bin/
    fi
  SHELL
end

def install_coverity(machine)
  machine.vm.provision "file", source: "~/Projects/cov-analysis-linux64-2017.07.tar.gz", destination: "/tmp/cov-analysis.tgz"

  machine.vm.provision "shell", inline: <<-SHELL
    cd /tmp
    tar -xzvf cov-analysis.tgz  --strip-components 1 -C /usr/local/
  SHELL
end

def configure_machine(machine)
  # config.vm.network :forwarded_port, guest: 52698, host: 52698

  machine.vm.provision "shell", inline: <<-SHELL
    sudo apt-get update
  SHELL

  machine.vm.provision "shell", privileged: false, path: "./script/vagrant-setup.sh"

  install_rmate(machine) if File.exist?(File.expand_path("~/bin/rmate"))
  install_coverity(machine)
end

MACHINES = [
  {name: "ubuntu-14.04", image: "bento/ubuntu-14.04"},
  {name: "ubuntu-16.04", image: "bento/ubuntu-16.04"},
  {name: "ubuntu-18.04", image: "bento/ubuntu-18.04"},
  {name: "ubuntu-trusty32", image: "ubuntu/trusty32"},
  {name: "fedora-28",    image: "generic/fedora28"},
  {name: "centos-6",     image: "generic/centos6"},
]


Vagrant.configure("2") do |config|

  config.vm.synced_folder ".", "/libgit2-src", SharedFoldersEnableSymlinksCreate: false

  config.vm.provider "virtualbox" do |vb|
    vb.memory = "1024"
  end

  MACHINES.each do |spec|
    config.vm.define spec[:name] do |machine|
      machine.vm.box = spec[:image]

      configure_machine(machine)
    end
  end

end
