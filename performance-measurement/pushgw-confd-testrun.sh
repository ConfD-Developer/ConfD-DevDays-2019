#!/bin/bash
PGW_VERSION=${PGW_VERSION}
NEX_VERSION=${NEX_VERSION}
PROM_VERSION=${PROM_VERSION}
GRFN_VERSION=${GRFN_VERSION}
VERBOSITY="normal"

/pushgateway-$PGW_VERSION.linux-amd64/pushgateway &
/node_exporter-$NEX_VERSION.linux-amd64/node_exporter &
/prometheus-$PROM_VERSION.linux-amd64/prometheus --config.file=prometheus.yml &
/grafana-${GRFN_VERSION}/bin/grafana-server --homepath=/grafana-${GRFN_VERSION} &

ID=0
echo "test-name,N,time(s),RSS(kB),HWM(kB)"
for i in {20000..1000000..20000}
do
    ID+=1
    NUM=$i
    make clean all > /dev/null
    ./cdbgen.py $NUM > init$NUM.xml
    kill -INT $MONITOR_PID # Stop the monitor while restarting ConfD
    make start > /dev/null
    ./pushgw-confd-monitor.sh &
    MONITOR_PID=$!
    /confd/bin/confd_cli -u admin -C << EOF
config
unhide debug
progress trace pm-demo enabled verbosity $VERBOSITY destination file progress$NUM.trace format log
commit
top
exit
exit
EOF
    START=$(date +%s)
    confd_load -m -l init$NUM.xml
    END=$(date +%s)
    TIME=$(($END-$START))
    PID=$(pidof confd)
    MEM=$(cat "/proc/$PID/status" | grep -A 1 VmHWM)
    arr=($MEM)
    HWM=$(echo ${arr[1]})
    RSS=$(echo ${arr[4]})
    DATA="confd_test_time{id=\""$ID"\",num=\""$NUM"\",test=\""maapi-load"\"} $TIME"$'\n'"confd_test_rss{id=\""$ID"\",num=\""$NUM"\",test=\""maapi-load"\"} $RSS"$'\n'"confd_test_hvm{id=\""$ID"\",num=\""$NUM"\",test=\""maapi-load"\"} $HWM"$'\n'
    curl -s -X POST -H  "Content-Type: text/plain" --data "$DATA" http://localhost:9091/metrics/job/testrun/instance/machine 2>&1 /dev/null
    echo "$NUM,$TIME,$RSS,$HWM,maapi-load" # > save.csv

    START=$(date +%s)
    confd_load -Fx -p /sys > /dev/null
    END=$(date +%s)
    TIME=$(($END-$START))
    PID=$(pidof confd)
    MEM=$(cat "/proc/$PID/status" | grep -A 1 VmHWM)
    arr=($MEM)
    HWM=$(echo ${arr[1]})
    RSS=$(echo ${arr[4]})
    DATA="confd_test_time{id=\""$ID"\",num=\""$NUM"\",test=\""maapi-save"\"} $TIME"$'\n'"confd_test_rss{id=\""$ID"\",num=\""$NUM"\",test=\""maapi-save"\"} $RSS"$'\n'"confd_test_hvm{id=\""$ID"\",num=\""$NUM"\",test=\""maapi-save"\"} $HWM"$'\n'
    curl -s -X POST -H  "Content-Type: text/plain" --data "$DATA" http://localhost:9091/metrics/job/testrun/instance/machine 2>&1 /dev/null
    echo "$NUM,$TIME,$RSS,$HWM,maapi-save" # > save.csv

    START=$(date +%s)
    netconf-console --get-config -s plain -x /sys > /dev/null
    END=$(date +%s)
    TIME=$(($END-$START))
    PID=$(pidof confd)
    MEM=$(cat "/proc/$PID/status" | grep -A 1 VmHWM)
    arr=($MEM)
    HWM=$(echo ${arr[1]})
    RSS=$(echo ${arr[4]})
    DATA="confd_test_time{id=\""$ID"\",num=\""$NUM"\",test=\""nc-getcfg"\"} $TIME"$'\n'"confd_test_rss{id=\""$ID"\",num=\""$NUM"\",test=\""nc-getcfg"\"} $RSS"$'\n'"confd_test_hvm{id=\""$ID"\",num=\""$NUM"\",test=\""nc-getcfg"\"} $HWM"$'\n'
    curl -s -X POST -H  "Content-Type: text/plain" --data "$DATA" http://localhost:9091/metrics/job/testrun/instance/machine 2>&1 /dev/null
    echo "$NUM,$TIME,$RSS,$HWM,nc-save" # > save.csv
done
done

tail -F progress$NUM.trace
