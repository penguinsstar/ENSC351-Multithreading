//============================================================================
//
//% Student Name 1: student1
//% Student 1 #: 123456781
//% Student 1 userid (email): stu1 (stu1@sfu.ca)
//
//% Student Name 2: student2
//% Student 2 #: 123456782
//% Student 2 userid (email): stu2 (stu2@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P3_<userid1>_<userid2>" (eg. P3_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : myIO.cpp
// Version     : September, 2019
// Description : Wrapper I/O functions for ENSC-351
// Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

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
    std::condition_variable tcdraincv;
    std::condition_variable readcv;
    int count = 0;
    int spair = 0;
};
std::mutex fileiolk;
std::vector<Utility*> socketpairs;

int mySocketpair( int domain, int type, int protocol, int des[2] )
{
    std::lock_guard<std::mutex> lk(fileiolk);
    int returnVal = socketpair(domain, type, protocol, des);
    if(returnVal == 0 && (socketpairs.size() <= (uint8_t)std::max(des[0], des[1]))){
        socketpairs.resize(std::max(des[0], des[1])+1, nullptr);
    }

    socketpairs[des[0]] = new Utility;
    socketpairs[des[0]]->spair = des[1];
    socketpairs[des[1]] = new Utility;
    socketpairs[des[1]]->spair = des[0];
    return returnVal;
}

int myOpen(const char *pathname, int flags, mode_t mode)
{
    std::lock_guard<std::mutex> lk(fileiolk);
    int newfd = open(pathname, flags, mode);
    if (newfd != -1){
        if((uint16_t)socketpairs.size() <= newfd){
            socketpairs.resize(newfd+1, nullptr);
        }
        socketpairs[newfd] = new Utility;

    }
    return newfd;
}

int myCreat(const char *pathname, mode_t mode)
{
    std::lock_guard<std::mutex> lk(fileiolk);
    int newfd = creat(pathname, mode);
    if (newfd != -1){
        if((uint16_t)socketpairs.size() <= newfd){
            socketpairs.resize(newfd+1, nullptr);
        }
        socketpairs[newfd] = new Utility;
    }
    return newfd;
}

ssize_t myRead( int fildes, void* buf, size_t nbyte )
{
    //    socketpairs.at(fildes)->tcdraincv.wait(lk, [&fildes]{return (socketpairs.at(fildes)->count>0); });

    // deal with reading from descriptors for files
    // myRead (for our socketpairs) reads a minimum of 1 byte
    if (socketpairs[fildes]->spair == 0){
        return read(fildes, buf, nbyte );
    }
    return myReadcond(fildes, buf, (int)nbyte, 1, 0, 0);

}

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
    int testbyteWrite;
    if (socketpairs[fildes]->spair == 0){
        testbyteWrite = write(fildes, buf, nbyte );
    }else{
        std::lock_guard<std::mutex> lk(socketpairs[socketpairs[fildes]->spair]->mut);

        testbyteWrite = write(fildes, buf, nbyte );
        if(testbyteWrite != -1){
            socketpairs[socketpairs[fildes]->spair]->count = socketpairs[socketpairs[fildes]->spair]->count + testbyteWrite;
            //wake up myreadcv
            socketpairs[socketpairs[fildes]->spair]->readcv.notify_one();
            COUT << "after writing: count = " << socketpairs[socketpairs[fildes]->spair]->count << " written = " << testbyteWrite << " at FD " << socketpairs[fildes]->spair << " to " << fildes << std::endl;
        }
    }

    return testbyteWrite;
//    return write(fildes, buf, nbyte );
}

int myClose( int fd )
{
    std::lock_guard<std::mutex> lk(socketpairs[fd]->mut);
    return close(fd);
}

int myTcdrain(int des)
{ //is also included for purposes of the course.
    std::unique_lock<std::mutex> lk(socketpairs[socketpairs[des]->spair]->mut);
    socketpairs[socketpairs[des]->spair]->tcdraincv.wait(lk, [&des]{return (socketpairs[socketpairs[des]->spair]->count<=0); });
    COUT << "TCDrain cond: " << (socketpairs[socketpairs[des]->spair]->count<=0) << std::endl;
    lk.unlock();
    return 0;
}

/* See:
 *  https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 *  */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{

    std::unique_lock<std::mutex> lk(socketpairs[des]->mut);

    //calls a wait if there is not enough data
    int curbufsize = 0;
    if (socketpairs[des]->count < min){
        curbufsize = socketpairs[des]->count;
        socketpairs[des]->count = 0;
        socketpairs[des]->tcdraincv.notify_one();
        socketpairs[des]->readcv.wait(lk, [&des, &curbufsize, &min]{return (((socketpairs[des]->count)+curbufsize)>=min); });
        socketpairs[des]->count+=curbufsize;
    }
    int testbyteRead = wcsReadcond(des, buf, n, min, time, timeout);
    if (testbyteRead != -1){
        socketpairs[des]->count -= testbyteRead;
        COUT << "after reading: count = " << socketpairs[des]->count << " read = " << testbyteRead << " at FD " << socketpairs[des]->spair << " from " << des << std::endl;
        if(socketpairs[des]->count <= 0){
            socketpairs[des]->tcdraincv.notify_one();
        }
    }
    lk.unlock();

    return testbyteRead;
}

