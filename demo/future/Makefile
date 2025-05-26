all:async promise package_task threadpool
async:async.cc
	g++ -std=c++11 $^ -o $@ -lpthread

promise:promise.cc
	g++ -std=c++11 $^ -o $@ -lpthread

package_task:package_task.cc
	g++ -std=c++11 $^ -o $@ -lpthread

threadpool:threadpool.cpp
	g++ -std=c++11 $^ -o $@ -lpthread

clean:
	rm -f async promise package_task threadpool