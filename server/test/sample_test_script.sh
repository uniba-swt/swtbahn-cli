echo "SWTbahn test script using the Python client"

train=$1
route=1

if test -z "$1"; then
	echo "Please specify a train!"
	exit
fi

cd ../../client
./swtbahn.py config localhost 8080 master
./swtbahn.py admin startup

sleep 0.5
./swtbahn.py monitor get-debug-info

while true; do
	./swtbahn.py driver grab $train
	./swtbahn.py monitor get-debug-info

#	./swtbahn.py driver request-route-id $route
#	./swtbahn.py monitor get-debug-info

	./swtbahn.py driver set-dcc-speed 13
	./swtbahn.py monitor get-debug-info

	./swtbahn.py driver set-dcc-speed 0
	./swtbahn.py monitor get-debug-info

#	./swtbahn.py controller release-route $route
#	./swtbahn.py monitor get-debug-info

	./swtbahn.py driver release
	./swtbahn.py monitor get-debug-info
done
