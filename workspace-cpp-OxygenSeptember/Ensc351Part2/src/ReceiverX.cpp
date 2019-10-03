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
// File Name   : ReceiverX.cpp
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <string.h> // for memset()
#include <fcntl.h>
#include <stdint.h>
#include <iostream>
#include "myIO.h"
#include "ReceiverX.h"
#include "VNPE.h"

//using namespace std;

ReceiverX::ReceiverX(int d, const char *fname, bool useCrc)
:PeerX(d, fname, useCrc), 
NCGbyte(useCrc ? 'C' : NAK),
goodBlk(false), 
goodBlk1st(false), 
syncLoss(false),
numLastGoodBlk(0)
{
}

void ReceiverX::receiveFile()
{
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	transferringFileD = PE2(myCreat(fileName, mode), fileName);

	// ***** improve this member function *****

	// below is just an example template.  You can follow a
	// 	different structure if you want.

	// inform sender that the receiver is ready and that the
	//		sender can send the first block
	sendByte(NCGbyte);
	errCnt = 0;
	int canCnt = 0;

	while(PE_NOT(myRead(mediumD, rcvBlk, 1), 1))
	{

		if(rcvBlk[0] == SOH){

		    getRestBlk();

            if (goodBlk == true && goodBlk1st == true){
                errCnt = 0;
                sendByte(ACK); // assume the expected block was received correctly.
                writeChunk();
            }
            else if (goodBlk == true && goodBlk1st == false){
                errCnt = 0;
                sendByte(ACK);
            }
            else{
                errCnt++;
                if (errCnt >= errB){
                    result = "ExcessiveErrors";
                    can8();
                    std::terminate();
                }
                sendByte(NAK);
            }
            canCnt = 0;
		}
		else if(rcvBlk[0] == EOT){
		    sendByte(NAK);
		    PE_NOT(myRead(mediumD, rcvBlk, 1), 1);
		    if(rcvBlk[0] == EOT){
		        (close(transferringFileD));
                result = "Done";
                sendByte(ACK);
                break;
		    }
		    else{
		        (close(transferringFileD));
		        std::cerr << "Receiver received totally unexpected char#" << rcvBlk[0] << std::endl;
		        can8();
		        std::terminate();
		    }

		}
		else if(rcvBlk[0] == CAN){
		    if(canCnt >= 1){
		        (close(transferringFileD));
		        result = "Transfer Cancelled by Sender";
		        std::terminate();
		    }
		    canCnt++;
		}
	};
//	// assume EOT was just read in the condition for the while loop
//	sendByte(NAK); // NAK the first EOT
//	PE_NOT(myRead(mediumD, rcvBlk, 1), 1); // presumably read in another EOT
//	(close(transferringFileD));
//	// check if the file closed properly.  If not, result should be something other than "Done".
//	result = "Done"; //assume the file closed properly.
//	sendByte(ACK); // ACK the second EOT
}

/* Only called after an SOH character has been received.
The function tries
to receive the remaining characters to form a complete
block.  The member
variable goodBlk1st will be made true if this is the first
time that the block was received in "good" condition.
*/
void ReceiverX::getRestBlk()
{
	// ********* this function must be improved ***********
	if (this->Crcflg){ //CRC
	    PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);
	    uint8_t myCrc16ns[2];
	    crc16ns((uint16_t*)&myCrc16ns, &rcvBlk[DATA_POS]);

	    uint8_t blkNC = ~rcvBlk[SOH_OH];
	    if (rcvBlk[SOH_OH + 1] == blkNC){ //if Blk# and complement match

	        if (uint8_t(numLastGoodBlk+1) == rcvBlk[SOH_OH]){ //if Blk# is 1+ the previous block

	            if (myCrc16ns[0] == rcvBlk[131] && myCrc16ns[1] == rcvBlk[132]){
                    goodBlk1st = goodBlk = true;
                    numLastGoodBlk++;
                }
                else{
                    goodBlk1st = goodBlk = false;
                }
	        }
	        else if(numLastGoodBlk == rcvBlk[SOH_OH]){ //if Blk# is the same as previous block
	            goodBlk = true;
	            goodBlk1st = false;
	        }
	        else{ //if Blk# is completely wrong fatal sync error
	            result = "Sync Loss";
                syncLoss = true;
                can8();
	            std::terminate();
	        }
	    }
	    else{ //if Blk# and complement does Not match
	        goodBlk1st = goodBlk = false;
	    }
	}
	else{ //checksum
	    PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CS, REST_BLK_SZ_CS, 0, 0), REST_BLK_SZ_CS);
        uint8_t myChecksum = rcvBlk[DATA_POS];
        for( int ii=DATA_POS+1; ii < DATA_POS+CHUNK_SZ; ii++ )
            myChecksum += rcvBlk[ii];

        uint8_t blkNC = ~rcvBlk[SOH_OH];
        if (rcvBlk[SOH_OH + 1] == blkNC){ //if Blk# and complement match

            if (uint8_t(numLastGoodBlk+1) == rcvBlk[SOH_OH]){ //if Blk# is 1+ the previous block

                if (myChecksum == rcvBlk[131]){
                    goodBlk1st = goodBlk = true;
                    numLastGoodBlk++;
                }
                else{
                    goodBlk1st = goodBlk = false;
                }
            }
            else if(numLastGoodBlk == rcvBlk[SOH_OH]){ //if Blk# is the same as previous block
                goodBlk = true;
                goodBlk1st = false;
            }
            else{ //if Blk# is completely wrong fatal sync error
                result = "Sync Loss";
                syncLoss = true;
                can8();
                std::terminate();
            }
        }
        else{ //if Blk# and complement does Not match
            goodBlk1st = goodBlk = false;
        }

	}

//	goodBlk1st = goodBlk = true;
}

//Write chunk (data) in a received block to disk
void ReceiverX::writeChunk()
{
	PE_NOT(write(transferringFileD, &rcvBlk[DATA_POS], CHUNK_SZ), CHUNK_SZ);
}

//Send 8 CAN characters in a row to the XMODEM sender, to inform it of
//	the cancelling of a file transfer
void ReceiverX::can8()
{
	// no need to space CAN chars coming from receiver in time
    char buffer[CAN_LEN];
    memset( buffer, CAN, CAN_LEN);
    PE_NOT(myWrite(mediumD, buffer, CAN_LEN), CAN_LEN);
}
