#!/bin/sh
set -e

# GCE can work with our normal file system, but it needs an "init system".
# Here is a better place to install this script so it doesn't get put in real
# images built from the GCE Python package.
cat << 'EOF' > init.sh && chmod 755 init.sh
#!/bin/bash -ex

# Write a configuration template if it does not exist.
[ -e /etc/default/instance_configs.cfg.template ] ||
echo -e > /etc/default/instance_configs.cfg.template \
    '[InstanceSetup]\nset_host_keys = false'

# Run the initialization scripts.
/usr/bin/google_instance_setup
/usr/bin/google_metadata_script_runner --script-type startup

# Handle the signal to shut down this service.
trap 'stopping=1 ; kill "${daemon_pids[@]}" || :' SIGTERM

# Fork the daemon processes.
daemon_pids=()
for d in accounts clock_skew network
do
        /usr/bin/google_${d}_daemon & daemon_pids+=($!)
done

# Notify the host that everything is running.
NOTIFY_SOCKET=/run/systemd/notify /usr/bin/systemd-notify --ready

# Pause while the daemons are running, and stop them all when one dies.
wait -n "${daemon_pids[@]}" || :
kill "${daemon_pids[@]}" || :

# If a daemon died while we're not shutting down, fail.
test -n "$stopping" || exit 1

# Otherwise, run the shutdown script before quitting.
exec /usr/bin/google_metadata_script_runner --script-type shutdown
EOF

# Disable PAM checks in the container.
rm -f usr/lib/pam.d/*
cat << 'EOF' > usr/lib/pam.d/other
account optional pam_permit.so
auth optional pam_permit.so
password optional pam_permit.so
session optional pam_permit.so
EOF

# Don't bundle these paths, since they are useless to us.
mv usr/lib64/systemd/lib*.so* usr/lib64/
rm -fr boot etc/* usr/lib64/systemd var/db/pkg
