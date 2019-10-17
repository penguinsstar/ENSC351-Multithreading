/* Wrapper functions for ENSC-351, Simon Fraser University, By
 *  - Craig Scratchley
 * 
 * These functions may be re-implemented later in the course.
 */

#include <unistd.h>			// for read/write/close
#include <fcntl.h>			// for open/creat
#include <sys/socket.h> 		// for socketpair
#include "SocketReadcond.h"
#include <thread> //for testing
#include "myIO.h"
#include <mutex>
#include <condition_variable>
#include "AtomicCOUT.h"
#include <vector>
#include <algorithm>

class Utility{
public:
    std::mutex mut;
    std::condition_variable cv;
    int count = 0;
    int spair = 0;

};
std::vector<Utility*> socketpairs;

//Utility ins;

int mySocketpair( int domain, int type, int protocol, int des[2] )
{
    int returnVal = socketpair(domain, type, protocol, des);
    COUT << "FD1: " << des[0] << std::endl;
    COUT << "FD2: " << des[1] << std::endl;
    if(returnVal == 0 && (socketpairs.size() < (uint8_t)std::max(des[0], des[1]))){
        socketpairs.resize(std::max(des[0], des[1])+1, nullptr);
    }
    socketpairs.at(des[0]) = new Utility;
    socketpairs.at(des[0])->spair = des[1];
    socketpairs.at(des[1]) = new Utility;
    socketpairs.at(des[1])->spair = des[0];
    return returnVal;
}

int myOpen(const char *pathname, int flags, mode_t mode)
{
	return open(pathname, flags, mode);
}

int myCreat(const char *pathname, mode_t mode)
{
	return creat(pathname, mode);
}

ssize_t myRead( int fildes, void* buf, size_t nbyte )
{
    //if(ins.count !=)

//    COUT << "Thread " << std::this_thread::get_id() << "getting Read mutex" << std::endl;
    std::lock_guard<std::mutex> lk(socketpairs.at(fildes)->mut);
    // deal with reading from descriptors for files
    // myRead (for our socketpairs) reads a minimum of 1 byte
    int testbyteRead = myReadcond(fildes, buf, (int)nbyte, 1, 0, 0);
    if (testbyteRead != -1){
        socketpairs.at(fildes)->count = socketpairs.at(fildes)->count - testbyteRead;
        COUT << "after reading: " << socketpairs.at(fildes)->count << " read " << testbyteRead << " at FD " << fildes << std::endl;
        if(socketpairs.at(fildes)->count == 0){
            socketpairs.at(fildes)->cv.notify_one();
        }
    }

    return testbyteRead;
	//return read(fildes, buf, nbyte );
}

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
//    COUT << "Thread " << std::this_thread::get_id() << "getting Write mutex" << std::endl;
    std::lock_guard<std::mutex> lk(socketpairs.at(fildes)->mut);
    int testbyteWrite = write(fildes, buf, nbyte );
    if(testbyteWrite != -1){
        socketpairs.at(socketpairs.at(fildes)->spair)->count = socketpairs.at(socketpairs.at(fildes)->spair)->count + testbyteWrite;

        COUT << "after writing: " << socketpairs.at(socketpairs.at(fildes)->spair)->count << " written " << testbyteWrite << " at FD " << fildes << std::endl;
    }

	return testbyteWrite;
}

int myClose( int fd )
{
	return close(fd);
}

int myTcdrain(int des)
{ //is also included for purposes of the course.
    std::unique_lock<std::mutex> lk(socketpairs.at(des)->mut);
//    socketpairs.at(des)->cv.wait(lk, [](int des){return (socketpairs.at(des)->count==0);});
    socketpairs.at(des)->cv.wait(lk, [&des]{return (socketpairs.at(des)->count==0); });
    COUT << "TCDrain cond: " << (socketpairs.at(des)->count==0) << std::endl;
	return 0;
}

int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{
	return wcsReadcond(des, buf, n, min, time, timeout );
}

