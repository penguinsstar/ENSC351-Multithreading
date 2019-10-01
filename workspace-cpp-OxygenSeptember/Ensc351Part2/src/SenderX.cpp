//============================================================================
//
//% Student Name 1: Allan Tsai
//% Student 1 #: 301314198
//% Student 1 userid (email): ata87 (ata87@sfu.ca)
//
//% Student Name 2: Daniel Wan
//% Student 2 #: 301318090
//% Student 2 userid (email): dwa90 (dwa90@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None') None
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  None
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P2_<userid1>_<userid2>" (eg. P2_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : SenderX.cpp
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <iostream>
#include <stdint.h> // for uint8_t
#include <string.h> // for memset()
#include <fcntl.h>	// for O_RDONLY
#include <thread>
#include "myIO.h"
#include "SenderX.h"
#include "VNPE.h"

using namespace std;

SenderX::SenderX(const char *fname, int d)
:PeerX(d, fname),
 bytesRd(-1),
 firstCrcBlk(true),
 blkNum(0)  	// but first block sent will be block #1, not #0
{
}

//-----------------------------------------------------------------------------

// get rid of any characters that may have arrived from the medium.
void SenderX::dumpGlitches()
{
	const int dumpBufSz = 20;
	char buf[dumpBufSz];
	int bytesRead;
	while (dumpBufSz == (bytesRead = PE(myReadcond(mediumD, buf, dumpBufSz, 0, 0, 0))));
}

// Send the block, less the block's last byte, to the receiver.
// Returns the block's last byte.

uint8_t SenderX::sendMostBlk(blkT blkBuf)
//uint8_t SenderX::sendMostBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
	const int mostBlockSize = (this->Crcflg ? BLK_SZ_CRC : BLK_SZ_CS) - 1;
	PE_NOT(myWrite(mediumD, blkBuf, mostBlockSize), mostBlockSize);
	return *(blkBuf + mostBlockSize);
}

// Send the last byte of a block to the receiver
void
SenderX::sendLastByte(uint8_t lastByte)
{
	PE(myTcdrain(mediumD)); // wait for previous part of block to be completely drained from the descriptor
	dumpGlitches();			// dump any received glitches

	PE_NOT(myWrite(mediumD, &lastByte, sizeof(lastByte)), sizeof(lastByte));
}

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block
was prepared or if the input file is empty (i.e. has 0 length).
*/
void SenderX::genBlk(blkT blkBuf)
//void SenderX::genBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
	//read data and store it directly at the data portion of the buffer
	bytesRd = PE(read(transferringFileD, &blkBuf[DATA_POS], CHUNK_SZ ));
	if (bytesRd>0) {
		blkBuf[0] = SOH; // can be pre-initialized for efficiency
		//block number and its complement
		uint8_t nextBlkNum = blkNum + 1;
		blkBuf[SOH_OH] = nextBlkNum;
		blkBuf[SOH_OH + 1] = ~nextBlkNum;
		if (this->Crcflg) {
			/*add padding*/
			if(bytesRd<CHUNK_SZ)
			{
				//pad ctrl-z for the last block
				uint8_t padSize = CHUNK_SZ - bytesRd;
				memset(blkBuf+DATA_POS+bytesRd, CTRL_Z, padSize);
			}

			/* calculate and add CRC in network byte order */
			crc16ns((uint16_t*)&blkBuf[PAST_CHUNK], &blkBuf[DATA_POS]);
		}
		else {
			//checksum
			blkBuf[PAST_CHUNK] = blkBuf[DATA_POS];
			for( int ii=DATA_POS + 1; ii < DATA_POS+bytesRd; ii++ )
				blkBuf[PAST_CHUNK] += blkBuf[ii];

			//padding
			if( bytesRd < CHUNK_SZ )  { // this line could be deleted
				//pad ctrl-z for the last block
				uint8_t padSize = CHUNK_SZ - bytesRd;
				memset(blkBuf+DATA_POS+bytesRd, CTRL_Z, padSize);
				blkBuf[PAST_CHUNK] += CTRL_Z * padSize;
			}
		}
	}
}

/* tries to prepare the first block.
*/
void SenderX::prep1stBlk()
{
	// **** this function will need to be modified ****
//    genBlk(blkBufs[0]);
    genBlk(blkBuf);
    memcpy(blkBufs[0], blkBuf, BLK_SZ_CRC);

//    std::cout << "Size of:" << sizeof(blkBufs[0]) << std::endl;
//    std::cout << "Printing out content of blkBufs[0]" << std::endl;
//    std::cout << blkBufs[0] << std::endl;
//    std::cout << "Printing out content of blkBufs[1]" << std::endl;
//    std::cout << blkBufs[1] << std::endl;
}

/* refit the 1st block with a checksum
*/
void
SenderX::cs1stBlk()
{
	// **** this function will need to be modified ****
    blkBuf[PAST_CHUNK] = 0;
    for( int ii=DATA_POS + 1; ii < DATA_POS+bytesRd; ii++ )
        blkBuf[PAST_CHUNK] += blkBuf[ii];
    memcpy(blkBufs[0], blkBuf, BLK_SZ_CS);
    memcpy(blkBufs[1], blkBuf, BLK_SZ_CS);
}

/* while sending the now current block for the first time, prepare the next block if possible.
*/
void SenderX::sendBlkPrepNext()
{
	// **** this function will need to be modified ****
	blkNum ++; // 1st block about to be sent or previous block ACK'd
	uint8_t lastByte = sendMostBlk(blkBufs[0]);
	sendLastByte(lastByte);
	genBlk(blkBufs[1]);
}

// Resends the block that had been sent previously to the xmodem receiver
void SenderX::resendBlk()
{
	// resend the block including the checksum or crc16
	//  ***** You will have to write this simple function *****
//    PE_NOT(myWrite(mediumD, &blkBuf, 2), 2);
    if (Crcflg == true){
        PE_NOT(myWrite(mediumD, blkBufs[0], BLK_SZ_CRC), BLK_SZ_CRC);
    }
    else if(Crcflg == false){
        PE_NOT(myWrite(mediumD, blkBufs[0], BLK_SZ_CS), BLK_SZ_CS);
    }
}

//Send CAN_LEN copies of CAN characters in a row (in pairs spaced in time) to the
//  XMODEM receiver, to inform it of the canceling of a file transfer.
//  There should be a total of (canPairs - 1) delays of
//  ((TM_2CHAR + TM_CHAR)/2 * mSECS_PER_UNIT) milliseconds
//  between the pairs of CAN characters.
void SenderX::can8()
{
    for(int i = 0; i < 3; i++){
        blkBuf[0] = CAN;
        blkBuf[1] = CAN;
        PE_NOT(myWrite(mediumD, &blkBuf, 2), 2);
        std::this_thread::sleep_for (std::chrono::milliseconds(1500));
        //sleep(1.5); //1500ms
    }
    blkBuf[0] = CAN;
    blkBuf[1] = CAN;
    PE_NOT(myWrite(mediumD, &blkBuf, 2), 2);
	//  ***** You will have to write this simple function *****
	// use the C++11/14 standard library to generate the delays
}

void SenderX::sendFile()
{
	transferringFileD = myOpen(fileName, O_RDONLY, 0);
	if(transferringFileD == -1) {
		can8();
		cout /* cerr */ << "Error opening input file named: " << fileName << endl;
		result = "OpenError";
	}
	else {
		//blkNum = 0; // but first block sent will be block #1, not #0
		prep1stBlk();
		errCnt = 0;
		// ***** modify the below code according to the protocol *****
		// below is just a starting point.  You can follow a
		// 	different structure if you want.
		char byteToReceive;
		PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get a 'C'
		Crcflg = true;
		if(byteToReceive == NAK){ //in the case we get a NAK instead (for checksum)
            Crcflg = false;
            firstCrcBlk = false;
            cs1stBlk();
        }
		sendBlkPrepNext();

		while (bytesRd) {
			// assuming below we get an ACK
			PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
			if (byteToReceive == ACK){
			    memcpy(blkBufs[0], blkBufs[1], sizeof(blkBufs[1]));
			    sendBlkPrepNext();
			    errCnt = 0;
			    firstCrcBlk = false;
			}
			else if((byteToReceive == NAK || (byteToReceive == 'C' && firstCrcBlk)) && errCnt < errB){
			    resendBlk();
			    errCnt++;
			}
			else if(byteToReceive == CAN){
			    result = "RcvCancelled";
			    //No clearCan() function
			    PE(myClose(transferringFileD));
			    std::terminate();
			}
		}
		sendByte(EOT); // send the first EOT
		PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get a NAK
		if(byteToReceive == NAK){
		    sendByte(EOT); // send the second EOT
            PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get an ACK
            if(byteToReceive == ACK){
                result = "Done";
            }
            else if(byteToReceive == NAK){
                while(errCnt < errB){
                    sendByte(EOT);
                    errCnt++;
                }
            }
		}
		else if(byteToReceive == ACK){
		    result = "1st EOT ACK'd";
		}

		PE(myClose(transferringFileD));
		/*
		if (-1 == myClose(transferringFileD))
			VNS_ErrorPrinter("myClose(transferringFileD)", __func__, __FILE__, __LINE__, errno);
		*/
	}
}

