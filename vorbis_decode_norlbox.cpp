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

ogg_int16_t convbuffer[4096]; /* take 8k out of the data segment, not the stack */
int convsize=4096;

//     Tested without the follwing line and functionality did not change
// extern void _VDBG_dump(void);


int main(){
  ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
  ogg_stream_state os; /* take physical pages, weld into a logical
                          stream of packets */
  ogg_page         og; /* one Ogg bitstream page. Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */

  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                          settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
  
  char *buffer;
  int  bytes;
  long first_granule_pos = 0, last_granule_pos = 0, rate=0;
  float time=0;
  std::ifstream file("test_song.ogg",std::ios::in | std::ios::binary);
  std::ofstream outfile("decoded_op");


  /********** Decode setup ************/

  ogg_sync_init(&oy); /* Now we can read pages */
  
  while(1){ /* we repeat if the bitstream is chained */
    int eos=0;
    int i;

    /* grab some data at the head of the stream. We want the first page
       (which is guaranteed to be small and only contain the Vorbis
       stream initial header) We need the first page to get the stream
       serialno. */

    /* submit a 4k block to libvorbis' Ogg layer */
    buffer=ogg_sync_buffer(&oy,4096);
    file.read(buffer,4096);
    bytes=file.gcount();
    ogg_sync_wrote(&oy,bytes);
    
    fprintf(stdout,"Here in #75");
    /* Get the first page. */
    if(ogg_sync_pageout(&oy,&og)!=1){ 
      /* have we simply run out of data?  If so, we're done. */
     fprintf(stdout,"Here in #79");
     fprintf(stdout,"bytes val=%d",bytes);
     
      if(bytes<4096)break;
      
      /* error case.  Must not be Vorbis data */
      fprintf(stdout,"Input does not appear to be an Ogg bitstream.\n");
      exit(1);
    }
    fprintf(stdout,"Header len - ID hdr = %ld\n",og.header_len);
    /* Get the serial number and set up the rest of decode. */
    /* serialno first; use it to set up a logical stream */
    ogg_stream_init(&os,ogg_page_serialno(&og));
    fprintf(stdout,"OGG PAGE SERIAL no. = %d\n\n",ogg_page_serialno(&og));
    
    /* extract the initial header from the first page and verify that the
       Ogg bitstream is in fact Vorbis data */
    
    /* I handle the initial header first instead of just having the code
       read all three Vorbis headers at once because reading the initial
       header is an easy way to identify a Vorbis bitstream and it's
       useful to see that functionality seperated out. */
    
    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);
    if(ogg_stream_pagein(&os,&og)<0){ 
      /* error; stream version mismatch perhaps */
      fprintf(stdout,"Error reading first page of Ogg bitstream data.\n");
      exit(1);
    }
    fprintf(stdout, "BEGINNING INITIAL: %d\n", ogg_page_bos(&og));
    
    if(ogg_stream_packetout(&os,&op)!=1){ 
      /* no page? must not be vorbis */
      fprintf(stdout,"Error reading initial header packet.\n");
      exit(1);
    }
    fprintf(stdout,"Granule pos in initial header: %ld\n",op.granulepos);
    if(vorbis_synthesis_headerin(&vi,&vc,&op)<0){ 
      /* error case; not a vorbis header */
      fprintf(stdout,"This Ogg bitstream does not contain Vorbis "
              "audio data.\n");
      exit(1);
    }
    fprintf(stdout,"COmment info header in!");
    fprintf(stdout,"Channels=%d",vi.channels);
    fprintf(stdout,"BR Upper=%ld",vi.bitrate_upper);
    fprintf(stdout,"BR Upper=%ld",vi.bitrate_nominal);
    fprintf(stdout,"BR Upper=%ld",vi.bitrate_lower);
    fprintf(stdout,"BR Upper=%ld",vi.bitrate_window);
    /* At this point, we're sure we're Vorbis. We've set up the logical
       (Ogg) bitstream decoder. Get the comment and codebook headers and
       set up the Vorbis decoder */
    
    /* The next two packets in order are the comment and codebook headers.
       They're likely large and may span multiple pages. Thus we read
       and submit data until we get our two packets, watching that no
       pages are missing. If a page is missing, error out; losing a
       header page is the only place where missing data is fatal. */
    
    i=0;
    fprintf(stdout,"AUDIO DATA STARTS HERE !");
    while(i<2){
      while(i<2){
        int result=ogg_sync_pageout(&oy,&og);
        if(result==0)break; /* Need more data */
        /* Don't complain about missing or corrupt data yet. We'll
           catch it at the packet output phase */
        if(result==1){
          ogg_stream_pagein(&os,&og); /* we can ignore any errors here
                                         as they'll also become apparent
                                         at packetout */
          while(i<2){
            result=ogg_stream_packetout(&os,&op);
            if(result==0)break;
            if(result<0){
              /* Uh oh; data at some point was corrupted or missing!
                 We can't tolerate that in a header.  Die. */
              fprintf(stdout,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }
            fprintf(stdout,"Granule pos comment &  info hdr: %ld\n",op.granulepos);
            result=vorbis_synthesis_headerin(&vi,&vc,&op);
            if(result<0){
              fprintf(stdout,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }
            i++;
          }
        }
      }
      /* no harm in not checking before adding more */
      buffer=ogg_sync_buffer(&oy,4096);
      file.read(buffer,4096);
      bytes=file.gcount();
      if(bytes==0 && i<2){
        fprintf(stdout,"End of file before finding all Vorbis headers!\n");
        exit(1);
      }
      ogg_sync_wrote(&oy,bytes);
    }
    
    /* Throw the comments plus a few lines about the bitstream we're
       decoding */
    {
      char **ptr=vc.user_comments;
      while(*ptr){
        fprintf(stdout,"%s\n",*ptr);
        ++ptr;
      }
      fprintf(stdout,"\nBitstream is %d channel, %ldHz\n",vi.channels,vi.rate);
      rate=vi.rate;
      fprintf(stdout,"Encoded by: %s\n\n",vc.vendor);
    }
    
    convsize=4096/vi.channels;

    /* OK, got and parsed all three headers. Initialize the Vorbis
       packet->PCM decoder. */
    if(vorbis_synthesis_init(&vd,&vi)==0){ /* central decode state */
      vorbis_block_init(&vd,&vb);          /* local state for most of the decode
                                              so multiple block decodes can
                                              proceed in parallel. We could init
                                              multiple vorbis_block structures
                                              for vd here */
      
      /* The rest is just a straight decode loop until end of stream */
      while(!eos){
        while(!eos){
          int result=ogg_sync_pageout(&oy,&og);
          if(result==0)break; /* need more data */
          if(result<0){ /* missing or corrupt data at this page position */
            fprintf(stdout,"Corrupt or missing data in bitstream; "
                    "continuing...\n");
          }else{
            ogg_stream_pagein(&os,&og); /* can safely ignore errors at
                                           this point */
            fprintf(stdout, "BEGINNING : %d\n", ogg_page_bos(&og));
            if(ogg_page_bos(&og)>0){
              fprintf(stdout,"\n!-!-!-!**************************************************************************************** THIS IS IT!!\n");
            }
            fprintf(stdout, "END: %d\n", ogg_page_eos(&og));
            while(1){
              result=ogg_stream_packetout(&os,&op);
              if(result==0)break; /* need more data */
              if(result<0){ /* missing or corrupt data at this page position */
                /* no reason to complain; already complained above */
              }else{
                /* we have a packet.  Decode it */
                float **pcm;
                int samples;
                fprintf(stdout,"Granule pos in audio packet: %ld\n",op.granulepos); 
                if(first_granule_pos == 0 && (op.granulepos!=-1 && op.granulepos!=0)){
                  first_granule_pos = op.granulepos;
                }             
                else{
                  last_granule_pos=op.granulepos;
                }
                fprintf(stdout,"--------------------------------------------\n\n\n");
                if(vorbis_synthesis(&vb,&op)==0) /* test for success! */
                  vorbis_synthesis_blockin(&vd,&vb);
                /* 
                   
                **pcm is a multichannel float vector.  In stereo, for
                example, pcm[0] is left, and pcm[1] is right.  samples is
                the size of each channel.  Convert the float values
                (-1.<=range<=1.) to whatever PCM format and write it out */
                
                while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0){
                  int j;
                  int clipflag=0;
                  int bout=(samples<convsize?samples:convsize);
                  
                  /* convert floats to 16 bit signed ints (host order) and
                     interleave */
//                   for(i=0;i<vi.channels;i++){
//                     ogg_int16_t *ptr=convbuffer+i;
//                     float  *mono=pcm[i];
// //                     for(j=0;j<bout;j++){
// // #if 1
// //                       int val=floor(mono[j]*32767.f+.5f);
// // #else /* optional dither */
// //                       int val=mono[j]*32767.f+drand48()-0.5f;
// // #endif
// //                       /* might as well guard against clipping */
// //                       // if(val>32767){
// //                       //   val=32767;
// //                       //   clipflag=1;
// //                       // }
// //                       // if(val<-32768){
// //                       //   val=-32768;
// //                       //   clipflag=1;
// //                       // }
// //                       *ptr=val;
// //                       ptr+=vi.channels;
// //                     }
//                   }
                  
                  // if(clipflag)
                  //   fprintf(stdout,"Clipping in frame %ld\n",(long)(vd.sequence));
                  
                  
                  // fwrite(convbuffer,2*vi.channels,bout,outfile);
                  
                  vorbis_synthesis_read(&vd,bout); /* tell libvorbis how
                                                      many samples we
                                                      actually consumed */
                }            
              }
            }
            if(ogg_page_eos(&og)){fprintf(stdout,"ENDS HERE MAN\n");eos=1;}
          }
        }
        if(!eos){
          buffer=ogg_sync_buffer(&oy,4096);
          file.read(buffer,4096);
	  bytes=file.gcount();
          ogg_sync_wrote(&oy,bytes);
          if(bytes==0)eos=1;
        }
      }
      
      /* ogg_page and ogg_packet structs always point to storage in
         libvorbis.  They're never freed or manipulated directly */
      
      vorbis_block_clear(&vb);
      vorbis_dsp_clear(&vd);
    }else{
      fprintf(stdout,"Error: Corrupt header during playback initialization.\n");
    }

    /* clean up this logical bitstream; before exit we see if we're
       followed by another [chained] */
    
    ogg_stream_clear(&os);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);  /* must be called last */
  }

  /* OK, clean up the framer */
  ogg_sync_clear(&oy);
  
  fprintf(stdout,"Done.\n");
  fprintf(stdout,"First granule pos %ld\n",first_granule_pos);

  fprintf(stdout,"Last granule pos %ld\n",last_granule_pos);
  
  fprintf(stdout,"Rate %ld\n",rate);

  time =  floor(last_granule_pos -  first_granule_pos);
  time = (float)time / (float)rate;
  fprintf(stdout,"Duration of audio file = %0.4fseconds \n",time);
  return(0);
}
