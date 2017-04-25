#!groovy

properties([
    buildDiscarder(logRotator(daysToKeepStr: '30', numToKeepStr: '50')),
    pipelineTriggers([pollSCM('H/20 * * * *')])
])

node('amd64 && coreos && sudo') {
    stage('Prepare') {
        sh 'sudo rm -fr coreos_developer_container.bin source wd'

        dir('source') {
            checkout scm
        }

        sh '''#!/bin/bash -ex
version_url="https://alpha.release.core-os.net/amd64-usr/current"
container_url="$version_url/coreos_developer_container.bin.bz2"

# Download and verify the developer container image.
gpg2 --recv-keys 04127D0BFABEC8871FFB2CCE50E0885593D2DCB4 || :
curl --fail --location "$container_url" |
tee >(bzip2 --decompress > coreos_developer_container.bin) |
gpg2 --verify <(curl --location --silent "$container_url.sig") -

# Define the Jenkins user in the container, and install build dependencies.
sudo mount -o offset=2097152,x-mount.mkdir coreos_developer_container.bin wd
trap 'sudo umount -d wd' EXIT
sudo cp /etc/resolv.conf wd/etc/
sudo chroot wd /bin/bash -ex << EOF
mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys
trap 'umount /dev /proc /sys' EXIT
echo $(getent passwd $(id -u)) >> /etc/passwd
echo $(getent group $(id -g)) >> /etc/group
usermod -aG sudo $USER
cat << EOG > /etc/pam.d/sudo
account optional pam_permit.so
auth optional pam_permit.so
password optional pam_permit.so
session optional pam_permit.so
EOG
emerge-gitclone
emerge -j4 -v dev-cpp/gmock dev-util/pkgconfig sys-devel/autoconf
mkdir /usr/src
EOF
sudo mv source wd/usr/src/update_engine
'''
    }

    stage('Build') {
        sh '''#!/bin/bash -ex
sudo mount -o offset=2097152 coreos_developer_container.bin wd
trap 'sudo umount -d wd' EXIT
sudo chroot --userspec=$(id -u):$(id -g) wd /bin/bash -ex << 'EOF'
sudo mount -t devtmpfs devtmpfs /dev
sudo mount -t proc proc /proc
sudo mount -t sysfs sysfs /sys
trap 'sudo umount /dev /proc /sys' EXIT
cd /usr/src/update_engine
./autogen.sh
./configure --enable-debug
make -j4 all
EOF
'''
    }

    stage('Test') {
        def rc = sh returnStatus: true, script: '''#!/bin/bash -ex
sudo mount -o offset=2097152 coreos_developer_container.bin wd
trap 'sudo umount -d wd' EXIT
sudo chroot --userspec=$(id -u):$(id -g) wd /bin/bash -ex << 'EOF'
sudo mount -t devtmpfs devtmpfs /dev
sudo mount -t proc proc /proc
sudo mount -t sysfs sysfs /sys
trap 'sudo umount /dev /proc /sys' EXIT
cd /usr/src/update_engine
make -j4 \
    update_engine_unittests \
    test_http_server \
    unittest_key.pub.pem \
    unittest_key2.pub.pem
GTEST_OUTPUT="xml:$PWD/user.xml" ./run_unittests_as_user
sudo GTEST_OUTPUT="xml:$PWD/root.xml" ./run_unittests_as_root
EOF
sudo mv wd/usr/src/update_engine source
'''

        archiveArtifacts 'source/root.xml,source/user.xml'

        junit 'source/root.xml,source/user.xml'

        currentBuild.result = rc == 0 ? 'SUCCESS' : 'FAILURE'
    }
}
