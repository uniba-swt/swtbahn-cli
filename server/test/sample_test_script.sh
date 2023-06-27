echo "SWTbahn test script using the Python client"

train=$1

if test -z "$1"; then
	echo "Please specify a train!"
	exit
fi

cd ../../client
./swtbahn config localhost 8080 master
./swtbahn admin startup

sleep 0.5
./swtbahn monitor get-debug-info

while true; do
	./swtbahn driver grab $train

	sleep 0.5
	./swtbahn monitor get-debug-info

	./swtbahn driver set-dcc-speed 13

	sleep 0.5
	./swtbahn monitor get-debug-info

	./swtbahn driver set-dcc-speed 0

	sleep 0.5
	./swtbahn monitor get-debug-info

	./swtbahn driver release

	sleep 0.5
	./swtbahn monitor get-debug-info
done
