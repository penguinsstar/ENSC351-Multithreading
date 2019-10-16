/* Wrapper functions for ENSC-351, Simon Fraser University, By
 *  - Craig Scratchley
 * 
 * These functions may be re-implemented later in the course.
 */

#include <unistd.h>			// for read/write/close
#include <fcntl.h>			// for open/creat
#include <sys/socket.h> 		// for socketpair
#include <thread> //for testing
#include "myIO.h"
#include "SocketReadcond.h"
#include <mutex>
#include <condition_variable>
#include "AtomicCOUT.h"
#include <vector>

class Utility{
public:
    std::mutex mut;
    std::condition_variable cv;
    int count = 0;
    int spair = 0;

};

std::vector<Utility> socketpairs(3);

Utility ins;

int mySocketpair( int domain, int type, int protocol, int des[2] )
{
	int returnVal = socketpair(domain, type, protocol, des);
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

    COUT << "Thread " << std::this_thread::get_id() << "getting Read mutex" << std::endl;
    std::lock_guard<std::mutex> lk(ins.mut);
    // deal with reading from descriptors for files
    // myRead (for our socketpairs) reads a minimum of 1 byte
    int testbyteRead = myReadcond(fildes, buf, (int)nbyte, 1, 0, 0);
    if (testbyteRead != -1){
        ins.count = ins.count - testbyteRead;
        COUT << "after reading: " << ins.count << std::endl;
        if(ins.count == 0){
            ins.cv.notify_one();
        }
    }

    return testbyteRead;
	//return read(fildes, buf, nbyte );
}

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
    COUT << "Thread " << std::this_thread::get_id() << "getting Write mutex" << std::endl;
    std::lock_guard<std::mutex> lk(ins.mut);
    int testbyteWrite = write(fildes, buf, nbyte );
    if(testbyteWrite != -1){
        ins.count = ins.count + testbyteWrite;

        COUT << "after writing: " << ins.count << std::endl;
    }

	return testbyteWrite;
}

int myClose( int fd )
{
	return close(fd);
}

int myTcdrain(int des)
{ //is also included for purposes of the course.
    std::unique_lock<std::mutex> lk(ins.mut);
    ins.cv.wait(lk, []{return (ins.count==0);});

	return 0;
}

int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{
	return wcsReadcond(des, buf, n, min, time, timeout );
}

