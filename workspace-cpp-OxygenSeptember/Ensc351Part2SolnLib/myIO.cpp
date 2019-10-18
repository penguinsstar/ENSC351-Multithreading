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

#include <unistd.h>         // for read/write/close
#include <fcntl.h>          // for open/creat
#include <sys/socket.h>         // for socketpair
#include "SocketReadcond.h"
#include <thread> //for testing
#include "myIO.h"
#include <mutex>
#include <condition_variable>
#include "AtomicCOUT.h"
#include <vector>
#include <algorithm>

int inputFD = 0;
int outputFD = 0;

class Utility{
public:
    std::mutex mut;
    std::condition_variable TCdrainCV;
    std::condition_variable readCV;
    int count = 0;
    int spair = 0;

};
std::vector<Utility*> socketpairs;

int mySocketpair( int domain, int type, int protocol, int des[2] )
{
    int returnVal = socketpair(domain, type, protocol, des);
    if(returnVal == 0 && (socketpairs.size() <= (uint8_t)std::max(des[0], des[1]))){
        socketpairs.resize(std::max(des[0], des[1])+1, nullptr);
    }
    COUT << "FD1: " << des[0] << std::endl;
    COUT << "FD2: " << des[1] << std::endl;
    socketpairs.at(des[0]) = new Utility;
    socketpairs.at(des[0])->spair = des[1];
    socketpairs.at(des[1]) = new Utility;
    socketpairs.at(des[1])->spair = des[0];
    return returnVal;
}

int myOpen(const char *pathname, int flags, mode_t mode)
{
    int fileDesc = open(pathname, flags, mode);
    inputFD = fileDesc;
    return fileDesc;
}

int myCreat(const char *pathname, mode_t mode)
{
    int fileDesc = creat(pathname, mode);
    outputFD = fileDesc;
    return fileDesc;
}

ssize_t myRead( int fildes, void* buf, size_t nbyte )
{
    return myReadcond(fildes, buf, nbyte, 1, 0, 0);
}

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
    if (fildes == outputFD)
        return write(fildes, buf, nbyte );

    std::lock_guard<std::mutex> lk(socketpairs.at(socketpairs.at(fildes)->spair)->mut);

    int testbyteWrite = write(fildes, buf, nbyte );
    if(testbyteWrite != -1){
        socketpairs.at(socketpairs.at(fildes)->spair)->count = socketpairs.at(socketpairs.at(fildes)->spair)->count + testbyteWrite;
        socketpairs.at(socketpairs.at(fildes)->spair)->readCV.notify_one();
    }
    COUT << "after writing: " << socketpairs.at(socketpairs.at(fildes)->spair)->count << " written " << testbyteWrite << " at FD " << socketpairs.at(fildes)->spair << " to " << fildes << std::endl;
	return testbyteWrite;
}

int myClose( int fd )
{
	return close(fd);
}

int myTcdrain(int des)
{ //is also included for purposes of the course.
    std::unique_lock<std::mutex> lk(socketpairs.at(socketpairs.at(des)->spair)->mut);
    socketpairs.at(des)->TCdrainCV.wait(lk, [&des]{return (socketpairs.at(socketpairs.at(des)->spair)->count==0); });
    COUT << "TCDrain cond: " << (socketpairs.at(des)->count==0) << std::endl;
	return 0;
}

/* See:
 *  https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 *  */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{
    if (des == inputFD)
        return read(des, buf, n );

    std::unique_lock<std::mutex> lk(socketpairs.at(des)->mut);
    socketpairs.at(des)->readCV.wait(lk, [&des]{return (socketpairs.at(des)->count>0); });

    // deal with reading from descriptors for files
    // myRead (for our socketpairs) reads a minimum of 1 byte
    int testbyteRead = wcsReadcond(des, buf, n, min, time, timeout );
    if (testbyteRead != -1){
        socketpairs.at(des)->count = socketpairs.at(des)->count - testbyteRead;
        if(socketpairs.at(des)->count == 0){
            socketpairs.at(des)->TCdrainCV.notify_one();
        }
    }
    COUT << "after reading: " << socketpairs.at(des)->count << " read " << testbyteRead << " at FD " << socketpairs.at(des)->spair << " from " << des << std::endl;
	return testbyteRead;
}

