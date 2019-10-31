#/bin/bash
trap "exit" SIGHUP SIGINT SIGTERM
#echo "*** MONITOR DATA CONFD PID: $PID ***"
while(true)
do
    PID=$(pidof confd)
    arr=($PID) # If there are more than one ConfD pid
    PID=$(echo ${arr[0]}) # ...select the first one
    if [ ! -z "$PID" ]
    then
        CPU=$(top -b -n 1 -p $PID | tail -n 1 | head -n 2 | awk '{print $9}')
        MEM=$(cat "/proc/$PID/status" | grep -A 1 VmHWM)
        arr=($MEM)
        HWM=$(echo ${arr[1]})
        RSS=$(echo ${arr[4]})
        DATA="confd_cpu_usage{process=\""confd"\"} $CPU"$'\n'"confd_rss{process=\""confd"\"} $RSS"$'\n'"confd_hvm{process=\""confd"\"} $HWM"$'\n'
        curl -s -X POST -H  "Content-Type: text/plain" --data "$DATA" http://localhost:9091/metrics/job/top/instance/machine 2>&1 /dev/null
        #echo "CPU(%),RSS(kB),HWM(kB)"
        #echo "$CPU,$RSS,$HWM" # > save.csv
        sleep 1
    fi
    done
done
