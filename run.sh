#set -x
FILENAME=$1
child=$!
echo $FILENAME
if [ -d "recv" ]; then
    rm -r recv/
fi
mkdir recv
make > /dev/null
./Server.out 10011 >> server.log &
./Client.out 10011 <<< $FILENAME >>client.log
kill -2 $(ps aux | grep '[S]erver.out' | awk '{print $2}')
#make clean