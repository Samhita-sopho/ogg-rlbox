/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2009             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: simple example decoder
 last mod: $Id$

 ********************************************************************/

/* Takes a vorbis bitstream from stdin and writes raw stereo PCM to
   stdout. Decodes simple and chained OggVorbis files from beginning
   to end. Vorbisfile.a is somewhat more complex than the code below.  */

/* Note that this is POSIX, not ANSI code */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vorbis/codec.h>
#include <fstream>
#include <iostream>
#include <string.h>
#include <cmath>
#include <fcntl.h>
#include "OggVorbisStructs.h"
// Configure RLBox
#define RLBOX_SINGLE_THREADED_INVOCATIONS

// Configure RLBox for noop sandbox
#define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol

#include "rlbox_noop_sandbox.hpp"
using sandbox_type_t = rlbox::rlbox_noop_sandbox;

#include "rlbox.hpp"

using namespace rlbox; 

template<typename T>                                                         
using tainted_data = rlbox::tainted<T, sandbox_type_t>;



rlbox_load_structs_from_library(oggvorbis);
// template<typename T>
// using tainted_img = rlbox::tainted<T, sandbox_type_t>;


ogg_int16_t convbuffer[4096]; /* take 8k out of the data segment, not the stack */
int convsize=4096;

// extern void _VDBG_dump(void);
#define fourK 4096
int main(){

  // Initializing a stream to read ogg file

  std::ifstream file("test_song.ogg",std::ios::in | std::ios::binary);

  
  // Creating sandbox here

  rlbox_sandbox<sandbox_type_t> sandbox;
  sandbox.create_sandbox();


  // ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
  // ogg_stream_state os; /* take physical pages, weld into a logical
  //                         stream of packets */
  // ogg_page         og; /* one Ogg bitstream page. Vorbis packets are inside */
  // ogg_packet       op; /* one raw packet of data for decode */

  // vorbis_info      vi; /* struct that stores all the static vorbis bitstream
  //                         settings */
  // vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  // vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  // vorbis_block     vb; /* local working space for packet->PCM decode */
  
  char *buffer;
  int  bytes;
  long first_granule_pos = 0, last_granule_pos = 0, rate=0;
  float time=0;
  
  auto oy_tainted = sandbox.malloc_in_sandbox<ogg_sync_state>();
  auto ogg_page_tainted = sandbox.malloc_in_sandbox<ogg_page>();
  auto ogg_stream_tainted = sandbox.malloc_in_sandbox<ogg_stream_state>();
  auto vi_tainted = sandbox.malloc_in_sandbox<vorbis_info>();
  auto vc_tainted = sandbox.malloc_in_sandbox<vorbis_comment>();
  auto ogg_packet_tainted = sandbox.malloc_in_sandbox<ogg_packet>();
  auto vorbis_dsp_tainted = sandbox.malloc_in_sandbox<vorbis_dsp_state>();
  auto vorbis_block_tainted = sandbox.malloc_in_sandbox<vorbis_block>();
  auto pcm = sandbox.malloc_in_sandbox<float **>();
  /********** Decode setup ************/

  // --OLD--  ogg_sync_init(&oy); 
  /* Now we can read pages */
  sandbox.invoke_sandbox_function(ogg_sync_init,oy_tainted);
  
  while(1){ /* we repeat if the bitstream is chained */
    int eos=0;
    int i;
    int func_return_status = 0;
    /* grab some data at the head of the stream. We want the first page
       (which is guaranteed to be small and only contain the Vorbis
       stream initial header) We need the first page to get the stream
       serialno. */

    /* submit a 4k block to libvorbis' Ogg layer */
    tainted_data<char*> audio_file_data = sandbox.invoke_sandbox_function(ogg_sync_buffer,oy_tainted,fourK);
  //   buffer=audio_file_data.copy_and_verify([](char* ret){
    
  // });
    // DO I NEED A COPY AND VERIFY HERE ??????????????????????????????????????
    char* buffer = audio_file_data.copy_and_verify_buffer_address(
        [](uintptr_t val) { return reinterpret_cast<char*>(val); },
        fourK);
    
    // -- OLD -- ogg_sync_buffer(&oy,4096);
    file.read(buffer,4096);
    bytes=file.gcount();

    sandbox.invoke_sandbox_function(ogg_sync_wrote, oy_tainted,
                         bytes)
              .unverified_safe_because("This is just to indicate to the ogg layer that we wrote");
    //--OLD-- ogg_sync_wrote(&oy,bytes);
    
    
    /* Get the first page. */
    // In the following condition it is ok to use unsafe unverified because the return value of ogg_sync_pageout is only 0/1/-1
    if(sandbox.invoke_sandbox_function(ogg_sync_pageout,oy_tainted,ogg_page_tainted).UNSAFE_unverified()!=1){ 
      /* have we simply run out of data?  If so, we're done. */
     fprintf(stdout,"Here in #79");
     fprintf(stdout,"bytes val=%d",bytes);
     
      if(bytes<4096)break;
      
      /* error case.  Must not be Vorbis data */
      fprintf(stdout,"Input does not appear to be an Ogg bitstream.\n");
      exit(1);
    }
    fprintf(stdout,"Header len - ID hdr = %ld\n",ogg_page_tainted->header_len.copy_and_verify([](long len){
      // if (len<0){return -1;}
      return len;
    }));
    
    // fprintf(stdout,"Header len - ID hdr = %ld\n",ogg_page_tainted->header_len.UNSAFE_unverified());
    
    /* Get the serial number and set up the rest of decode. */
    /* serialno first; use it to set up a logical stream */
    int serial_no = sandbox.invoke_sandbox_function(ogg_page_serialno,ogg_page_tainted).unverified_safe_because("We dont use the serial number to index into the stram yet!!");
    sandbox.invoke_sandbox_function(ogg_stream_init,ogg_stream_tainted,serial_no);
    
    
    fprintf(stdout,"OGG PAGE SERIAL no. = %d\n\n",serial_no);
    
    /* extract the initial header from the first page and verify that the
       Ogg bitstream is in fact Vorbis data */
    
    /* I handle the initial header first instead of just having the code
       read all three Vorbis headers at once because reading the initial
       header is an easy way to identify a Vorbis bitstream and it's
       useful to see that functionality seperated out. */





//     vorbis_info_init(&vi);
       sandbox.invoke_sandbox_function(vorbis_info_init, vi_tainted);
//     vorbis_comment_init(&vc);
       sandbox.invoke_sandbox_function(vorbis_comment_init, vc_tainted);
       
    if(sandbox.invoke_sandbox_function(ogg_stream_pagein,ogg_stream_tainted,ogg_page_tainted).UNSAFE_unverified()<0){ 
      /* error; stream version mismatch perhaps */
      fprintf(stdout,"Error reading first page of Ogg bitstream data.\n");
      exit(1);
    }
    fprintf(stdout, "BEGINNING INITIAL: %d\n", sandbox.invoke_sandbox_function(ogg_page_bos,ogg_page_tainted).copy_and_verify([](int ret){
      if(ret<0){
         exit(1);
      }
      return ret;
    }));

    
//     if(ogg_stream_packetout(&os,&op)!=1){ 
//       /* no page? must not be vorbis */
//       fprintf(stdout,"Error reading initial header packet.\n");
//       exit(1);
//     }


    // The function only returns a status code - No need of a copy and veriufy here
    int r1 = sandbox.invoke_sandbox_function(ogg_stream_packetout,ogg_stream_tainted,ogg_packet_tainted).UNSAFE_unverified();
    if(r1!=1){
      fprintf(stdout,"printing stream packet out return value %d\n",r1); 
      /* no page? must not be vorbis */
      fprintf(stdout,"Error reading initial header packet ***.\n");
      exit(1);
    }


//     fprintf(stdout,"Granule pos in initial header: %ld\n",op.granulepos);
//     if(vorbis_synthesis_headerin(&vi,&vc,&op)<0){ 
//       /* error case; not a vorbis header */
//       fprintf(stdout,"This Ogg bitstream does not contain Vorbis "
//               "audio data.\n");
//       exit(1);
//     }
r1 = sandbox.invoke_sandbox_function(vorbis_synthesis_headerin,vi_tainted,vc_tainted,ogg_packet_tainted).UNSAFE_unverified();
if(r1<0){ 
      fprintf(stdout,"printing vorbis_synthesis_headerin return value %d\n",r1); 

      /* no page? must not be vorbis */
      fprintf(stdout,"Error reading initial header packet #221.\n");
      exit(1);
    }
//     fprintf(stdout,"COmment info header in!");
    // fprintf(stdout,"Channels=%d",vi.channels);



//     /* At this point, we're sure we're Vorbis. We've set up the logical
//        (Ogg) bitstream decoder. Get the comment and codebook headers and
//        set up the Vorbis decoder */
    
//     /* The next two packets in order are the comment and codebook headers.
//        They're likely large and may span multiple pages. Thus we read
//        and submit data until we get our two packets, watching that no
//        pages are missing. If a page is missing, error out; losing a
//        header page is the only place where missing data is fatal. */
    
    i=0;
    fprintf(stdout,"AUDIO DATA STARTS HERE !");
    while(i<2){
      while(i<2){

        // Returns a status code only
        int result=sandbox.invoke_sandbox_function(ogg_sync_pageout,oy_tainted,ogg_page_tainted).copy_and_verify([](int ret){
          if (ret<-1 || ret>1)exit(0);
          return ret;
        });
        if(result==0)break; /* Need more data */
//         /* Don't complain about missing or corrupt data yet. We'll
//            catch it at the packet output phase */
        if(result==1){
          /* we can ignore any errors here
//                                          as they'll also become apparent
//                                          at packetout */
          sandbox.invoke_sandbox_function(ogg_stream_pagein,ogg_stream_tainted,ogg_page_tainted); 
          while(i<2){
            result=sandbox.invoke_sandbox_function(ogg_stream_packetout,ogg_stream_tainted,ogg_packet_tainted).UNSAFE_unverified();
            if(result==0)break;
            if(result<0){
              /* Uh oh; data at some point was corrupted or missing!
                 We can't tolerate that in a header.  Die. */
              fprintf(stdout,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }
            // fprintf(stdout,"Granule pos comment &  info hdr: %ld\n",op.granulepos);
            result=sandbox.invoke_sandbox_function(vorbis_synthesis_headerin,vi_tainted,vc_tainted,ogg_packet_tainted).UNSAFE_unverified();
            if(result<0){
              fprintf(stdout,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }
            i++;
          }
        }
      }
//       /* no harm in not checking before adding more */

      audio_file_data = sandbox.invoke_sandbox_function(ogg_sync_buffer,oy_tainted,fourK);
  
      buffer = audio_file_data.copy_and_verify_buffer_address(
      [](uintptr_t val) { return reinterpret_cast<char*>(val); },
      fourK);
      
      // buffer=ogg_sync_buffer(&oy,4096);
      file.read(buffer,4096);
      bytes=file.gcount();

      if(bytes==0 && i<2){
        fprintf(stdout,"End of file before finding all Vorbis headers!\n");
        exit(1);
      }
      sandbox.invoke_sandbox_function(ogg_sync_wrote,oy_tainted,bytes);
    }
    
//     /* Throw the comments plus a few lines about the bitstream we're
//        decoding */
    {
      // char **ptr=vc.user_comments;
      // while(*ptr){
      //   fprintf(stdout,"%s\n",*ptr);
      //   ++ptr;
      // }
      // WE just print them here so UNSAfe unverified is Ok.
      fprintf(stdout,"\nBitstream is %d channel, %ldHz\n",vi_tainted->channels.UNSAFE_unverified(),vi_tainted->rate.UNSAFE_unverified());
      rate=vi_tainted->rate.copy_and_verify([](long r){
        if(r<=0){
          fprintf(stdout,"Rate cannot be < =0, maybe some data is corrupted in the sandbox. exiting now!");
          exit(0);
        }
        return r;
      });

    }
      int channels = vi_tainted->channels.copy_and_verify([](int chnls){
        if(chnls<=0){
          fprintf(stdout,"Numbers of channels cannot be < =0, maybe some data is corrupted in the sandbox. exiting now!");
          exit(0);
        }
        return chnls;
      });
    convsize=4096/channels;

//     /* OK, got and parsed all three headers. Initialize the Vorbis
//        packet->PCM decoder. */
    if(sandbox.invoke_sandbox_function(vorbis_synthesis_init,vorbis_dsp_tainted,vi_tainted).UNSAFE_unverified()==0){ /* central decode state */
      sandbox.invoke_sandbox_function(vorbis_block_init,vorbis_dsp_tainted,vorbis_block_tainted);          /* local state for most of the decode
//                                               so multiple block decodes can
//                                               proceed in parallel. We could init
//                                               multiple vorbis_block structures
//                                               for vd here */
      
//       /* The rest is just a straight decode loop until end of stream */
      while(!eos){
        while(!eos){
          int result=sandbox.invoke_sandbox_function(ogg_sync_pageout,oy_tainted,ogg_page_tainted).UNSAFE_unverified();
          if(result==0)break; /* need more data */
          if(result<0){ /* missing or corrupt data at this page position */
            fprintf(stdout,"Corrupt or missing data in bitstream; "
                    "continuing...\n");
          }else{
            sandbox.invoke_sandbox_function(ogg_stream_pagein,ogg_stream_tainted,ogg_page_tainted); /* can safely ignore errors at
                                           this point */
        
           
            while(1){
              // result=ogg_stream_packetout(&os,&op);
              result = sandbox.invoke_sandbox_function(ogg_stream_packetout,ogg_stream_tainted,ogg_packet_tainted).UNSAFE_unverified();
              if(result==0)break; /* need more data */
              if(result<0){ /* missing or corrupt data at this page position */
                /* no reason to complain; already complained above */
              }else{
                /* we have a packet.  Decode it */
                
                int samples;
                fprintf(stdout,"Granule pos in audio packet: %ld\n",ogg_packet_tainted->granulepos.UNSAFE_unverified()); 

                // Getting the first granule position tha that is non-zero and non-negative
                if(first_granule_pos == 0 && (ogg_packet_tainted->granulepos.UNSAFE_unverified()!=-1 && ogg_packet_tainted->granulepos.UNSAFE_unverified()!=0)){
                  first_granule_pos = ogg_packet_tainted->granulepos.UNSAFE_unverified();
                }             
                else{
                  last_granule_pos=ogg_packet_tainted->granulepos.UNSAFE_unverified();
                }
                fprintf(stdout,"--------------------------------------------\n\n\n");
                if(sandbox.invoke_sandbox_function(vorbis_synthesis,vorbis_block_tainted,ogg_packet_tainted).UNSAFE_unverified()==0) /* test for success! */
                    sandbox.invoke_sandbox_function(vorbis_synthesis_blockin,vorbis_dsp_tainted,vorbis_block_tainted);
                /* 
                   
                **pcm is a multichannel float vector.  In stereo, for
                example, pcm[0] is left, and pcm[1] is right.  samples is
                the size of each channel.  Convert the float values
                (-1.<=range<=1.) to whatever PCM format and write it out */
                
                // Using UNSAFE unverified here because the while loop condition checks for a positive value.
                while((samples=sandbox.invoke_sandbox_function(vorbis_synthesis_pcmout,vorbis_dsp_tainted,pcm).UNSAFE_unverified())>0){
                  int j;
                  int clipflag=0;
                  int bout=(samples<convsize?samples:convsize);

                  
                  /* tell libvorbis how
//                                                       many samples we
//                                                       actually consumed */
                  sandbox.invoke_sandbox_function(vorbis_synthesis_read,vorbis_dsp_tainted,bout); 
                }            
              }
            }
//             if(ogg_page_eos(&og)){fprintf(stdout,"ENDS HERE MAN\n");eos=1;}
          }
        }
        if(!eos){
          // buffer=ogg_sync_buffer(&oy,4096);
          
          audio_file_data = sandbox.invoke_sandbox_function(ogg_sync_buffer,oy_tainted,fourK);
 
   
          buffer = audio_file_data.copy_and_verify_buffer_address(
                                  [](uintptr_t val) { return reinterpret_cast<char*>(val); },
                                                      fourK);

          file.read(buffer,4096);
	        bytes=file.gcount();

          sandbox.invoke_sandbox_function(ogg_sync_wrote,oy_tainted,bytes);

          // ogg_sync_wrote(&oy,bytes);
          if(bytes==0)eos=1;
        }
      }
      
//       /* ogg_page and ogg_packet structs always point to storage in
//          libvorbis.  They're never freed or manipulated directly */
      
//       vorbis_block_clear(&vb);
         sandbox.invoke_sandbox_function(vorbis_block_clear, vorbis_block_tainted);
//       vorbis_dsp_clear(&vd);
         sandbox.invoke_sandbox_function(vorbis_dsp_clear,vorbis_dsp_tainted);
    }else{
      fprintf(stdout,"Error: Corrupt header during playback initialization.\n");
    }

//     /* clean up this logical bitstream; before exit we see if we're
//        followed by another [chained] */
    
//     ogg_stream_clear(&os);
      sandbox.invoke_sandbox_function(ogg_stream_clear, ogg_stream_tainted);
      
//     vorbis_comment_clear(&vc);
      sandbox.invoke_sandbox_function(vorbis_comment_clear, vc_tainted);
      
//     vorbis_info_clear(&vi);  /* must be called last */
         sandbox.invoke_sandbox_function(vorbis_info_clear, vi_tainted);
       
  }

//   /* OK, clean up the framer */
//   ogg_sync_clear(&oy);
    sandbox.invoke_sandbox_function(ogg_sync_clear, oy_tainted);
    
  fprintf(stdout,"Done.\n");
  fprintf(stdout,"First granule pos %ld\n",first_granule_pos);

  fprintf(stdout,"Last granule pos %ld\n",last_granule_pos);
  
  fprintf(stdout,"Rate %ld\n",rate);

  time =  floor(last_granule_pos -  first_granule_pos);
  time = (float)time / (float)rate;
  fprintf(stdout,"Duration of audio file = %0.4fseconds \n",time);

  // sandbox.free_in_sandbox(oy_tainted);
  // sandbox.free_in_sandbox(ogg_page_tainted);
  // sandbox.free_in_sandbox(ogg_stream_tainted);
  // sandbox.free_in_sandbox(ogg_page_tainted);
  // sandbox.free_in_sandbox(ogg_packet_tainted);

  // sandbox.free_in_sandbox(vi_tainted);
  // sandbox.free_in_sandbox(vc_tainted);
  // sandbox.free_in_sandbox(vorbis_block_tainted);
  // sandbox.free_in_sandbox(vorbis_dsp_tainted);
  
  sandbox.destroy_sandbox();
  return(0);
}
