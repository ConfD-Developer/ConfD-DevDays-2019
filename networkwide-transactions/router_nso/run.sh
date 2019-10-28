#!/bin/bash
CONFD_VERSION=${CONFD_VERSION}
NSO_VERSION=${NSO_VERSION}
VERBOSITY="normal"

function version_gt() { test "$(printf '%s\n' "$@" | sort -V | head -n 1)" != "$1"; }

cd /router_confd
make all
echo "Starting CONFD-$CONFD_VERSION..."
make start

cd /router_nso
chmod 777 packages
make ncs
cp router-device_init.xml ncs-cdb
NSO52=5.2
if version_gt $NSO_VERSION $NSO52; then
    echo 'NSO version > 5.2 - using the built-in NETCONF NED builder'
    mv ncs.conf.in ncs.conf
    echo "Starting NSO-$NSO_VERSION..."
    make start

    # Add and build our nano service package too, reload together w device pkgs
    tar xvfz link.tar.gz -C packages
    make packages

    for i in 0 1 2
    do
        /nso/bin/ncs_cli -n -u admin -C << EOF
devtools true
config
devices device router$i ssh fetch-host-keys
netconf-ned-builder project router 1.0 device router$i local-user admin vendor tailf
commit
exit
exit
EOF
    done
    /nso/bin/ncs_cli -n -u admin -C << EOF
config
unhide debug
progress trace trans-demo enabled verbosity $VERBOSITY destination file progress.trace format log
commit
top
exit
devtools true
netconf-ned-builder project router 1.0 fetch-module-list overwrite
netconf-ned-builder project router 1.0 module * * deselect
netconf-ned-builder project router 1.0 module router* * select
netconf-ned-builder project router 1.0 module example-serial* * select
netconf-ned-builder project router 1.0 module tailf* * deselect
netconf-ned-builder project router 1.0 module ietf* * deselect
show netconf-ned-builder project router 1.0 module | nomore
netconf-ned-builder project router 1.0 build-ned
netconf-ned-builder project router 1.0 export-ned to-directory /router_nso/packages
exit
EOF
#    echo "Untar the NED package to use the YANG models for coverage metrics"
#    tar xfz packages/ncs-$NSO_VERSION-router-nc-1.0.tar.gz -C packages
#    mv packages/ncs-$NSO_VERSION-router-nc-1.0.tar.gz .
#    mv packages/router-nc-1.0 packages/router
else
    echo 'NSO version < 5.2 - using the Pioneer package to build the NETCONF NED'
    git clone https://github.com/NSO-developer/pioneer packages/pioneer
    cd packages/pioneer/src && make clean all && cd -
    echo "Starting NSO-$NSO_VERSION with our example specific ncs-conf..."
    /nso/bin/ncs
    for i in 0 1 2
    do
        /nso/bin/ncs_cli -n -u admin -C << EOF
devices device router$i ssh fetch-host-keys
commit
exit
EOF
    done
    /nso/bin/ncs_cli -n -u admin -C << EOF
devices device router$i pioneer yang fetch-list
devices device router$i pioneer yang disable name-pattern *
devices device router$i pioneer yang enable name-pattern router*
devices device router$i pioneer yang enable name-pattern example-serial*
devices device router$i pioneer yang show-list
devices device router$i pioneer yang download
devices device router$i pioneer yang check-dependencies
devices device router$i pioneer yang build-netconf-ned
devices device router$i pioneer yang install-netconf-ned
exit
exit
EOF
fi

/nso/bin/ncs_cli -n -u admin -C << EOF
packages reload
exit
EOF

for i in 0 1 2
do
    PORT=$((4565+$i*10))
    /confd/bin/confd_cli -P $PORT -n -u admin -C << EOF
config
unhide debug
progress trace trans-demo enabled verbosity $VERBOSITY destination file progress.trace format log
commit
top
exit
exit
EOF
    /nso/bin/ncs_cli -n -u admin -C << EOF
config
devices device router$i device-type netconf ned-id router-nc-1.0
commit
top
devices device router$i sync-from
devices device router$i ned-settings use-confirmed-commit true use-transaction-id true use-private-candidate true use-validate true
devices device router$i trace pretty
commit
exit
exit
EOF
done

printf "(add-to-list \'auto-mode-alist \'(\"\\\\\.trace\\\\\\\\\\'\" . nxml-mode))\n(add-hook \'nxml-mode-hook \\'auto-revert-tail-mode)\n(add-hook \'auto-revert-tail-mode-hook \'end-of-buffer)\n(add-hook 'find-file-hook (lambda () (highlight-regexp \"/router_confd/node[0-9]/progress.trace\")))\n(add-hook 'find-file-hook (lambda () (highlight-regexp \"/router_confd/node[0-9]/progress.trace\")))\n(add-hook 'find-file-hook (lambda () (highlight-regexp \"device router[0-9]\")))\n(add-hook 'find-file-hook (lambda () (highlight-regexp \"Initialization done\")))\n" > ~/.emacs

tail -F /router_nso/logs/netconf-router0.trace \
     -F /router_nso/logs/netconf-router1.trace \
     -F /router_nso/logs/netconf-router2.trace \
     -F /router_nso/logs/progress.trace \
     -F /router_confd/node0/progress.trace \
     -F /router_confd/node1/progress.trace \
     -F /router_confd/node2/progress.trace \
     2>&1 | tee -a netconf-routers.trace &

cd /router_nso/tools && make -f Makefile all && cd -
/router_nso/tools/confd_cmd -dd -c 'smx / 1 100 "suppress_defaults"' 2>&1 | tee -a netconf-routers.trace &
/router_nso/tools/confd_cmd -dd -p 4575 -c 'smx / 1 100 "suppress_defaults"' 2>&1 | tee -a netconf-routers.trace &
/router_nso/tools/confd_cmd -dd -p 4585 -c 'smx / 1 100 "suppress_defaults"' 2>&1 | tee -a netconf-routers.trace &

tail -F /router_nso/logs/ncs.log
exit
