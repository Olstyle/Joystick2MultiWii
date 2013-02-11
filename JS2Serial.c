/* abgeleitet von Galileo Computing sdl9.c *
* compileflags: 'sdl-config --libs` `sdl-config --cflags` */
#include <stdlib.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <termios.h>
#include "JS2Serial.h"


#include <fcntl.h>
#include <unistd.h>

/*********** RC alias *****************/
#define ROLL       0
#define PITCH      1
#define YAW        2
#define THROTTLE   3
#define AUX1       4
#define AUX2       5
#define AUX3       6
#define AUX4       7

static int16_t rcData[8];          // interval [1000;2000]

int serial;

static SDL_Joystick *js;
static void Joystick_Info (void) 
{
  int num_js, i;
  num_js = SDL_NumJoysticks ();
  printf ("Found %d Joystick(s)!\n",
     num_js);
  if (num_js == 0) {
    printf ("Could find a Joystick!\n");
    return;
  }
  /* Informationen zum Joystick */
  for (i = 0; i < num_js; i++) {
    js = SDL_JoystickOpen (i);
    if (js == NULL) {
      printf ("Cant open Joystick %d !\n", i);
    } 
    else {
      printf ("Joystick %d\n", i);
      printf ("\tName:       %s\n", SDL_JoystickName(i));
      printf ("\tAxis:       %i\n", SDL_JoystickNumAxes(js));
      printf ("\tTrackballs: %i\n", SDL_JoystickNumBalls(js));
      printf ("\tButtons:   %i\n",SDL_JoystickNumButtons(js));
      printf ("\tHats: %i\n",SDL_JoystickNumHats(js)); 
      SDL_JoystickClose (js);
    }
  }
}

int16_t parsetoMultiWii(Sint16 value)
{
	return (int16_t)(((((double)value)+32768.0)/65.536)+1000);
}

void readAxis(SDL_Event *event)
{
	SDL_Event myevent = (SDL_Event)*event;
	switch(myevent.jaxis.axis)
	{
		case ROLL_AXIS:
				rcData[0]=parsetoMultiWii(myevent.jaxis.value);
			break;
		case PITCH_AXIS:
				rcData[1]=parsetoMultiWii(myevent.jaxis.value);
			break;
		case YAW_AXIS:
				rcData[2]=parsetoMultiWii(myevent.jaxis.value);
			break;
		case THROTTLE_AXIS:
				rcData[3]=parsetoMultiWii(myevent.jaxis.value);
			break;
		case AUX1_AXIS:
				rcData[4]=parsetoMultiWii(myevent.jaxis.value);
			break;
		case AUX2_AXIS:
				rcData[5]=parsetoMultiWii(myevent.jaxis.value);
			break;
		case AUX3_AXIS:
				rcData[6]=parsetoMultiWii(myevent.jaxis.value);
			break;
		case AUX4_AXIS:
				rcData[7]=parsetoMultiWii(myevent.jaxis.value);
			break;	
		default:
			//do nothing
			break;
	}
}

static int eventloop_joystick (void) 
{
  SDL_Event event;
  while (SDL_PollEvent (&event)) 
  {
    switch (event.type) 
    {
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE)
			{
				printf ("Hit ESCAPE to exit\n");
				return 0;
			}
			break;
		case SDL_JOYAXISMOTION:
			//local
			//~ printf ("Joystick %d, Axis %d moved to %d\n",
			 //~ event.jaxis.which, event.jaxis.axis, event.jaxis.value);
			 //prepare for serial output
			 readAxis(&event);
			 return 2;
		  break;
		case SDL_JOYBUTTONUP:
		case SDL_JOYBUTTONDOWN:
			printf ("Joystick %i Button %i: %i\n",
				event.jbutton.which, event.jbutton.button,
				event.jbutton.state);
			break;
		case SDL_JOYHATMOTION:
			printf ("Joystick %i Hat %i: %i\n",
			event.jhat.which, event.jhat.hat, event.jhat.value);
			break;
		case SDL_QUIT:
			return 0;
    }
  }
  return 1;
}

void sendRC()
{
	uint8_t checksum=0;
	checksum^=16;
	checksum^=200;
	
	uint8_t i;
	uint8_t z;
	uint8_t outputbuffer[22];
	//header
	outputbuffer[0]='$';
	outputbuffer[1]='M';
	outputbuffer[2]='<';
	//size
	outputbuffer[3]=16;
	//message type
	outputbuffer[4]=200;
	
	z=0;
	for(i=5; i<21 ; i++)
	{
		//low byte
		outputbuffer[i]=(uint8_t)(rcData[z]&0xFF);
		checksum^=outputbuffer[i];
		i++;
		//high byte
		outputbuffer[i]=(uint8_t)(rcData[z]>>8);
		checksum^=outputbuffer[i];
		z++;
	}
	
	outputbuffer[21]=checksum;
	
	//~ //for debug use
	//~ printf("send: ");
	//~ int j;
	//~ for(j=0;j<22;j++)
	//~ {
		//~ printf("%d-",outputbuffer[j]);
	//~ }
	//~ printf("\n rc= ");
	//~ for(j=0;j<9;j++)
	//~ {
		//~ printf("%d-",rcData[j]);
	//~ }
	//~ printf("\n");
	
	write(serial,outputbuffer,22);
	//wait for serialport to send
	tcdrain(serial);
}


int main (void) 
{
	//prefill with middle position
	uint8_t i;
	for(i=0; i<8 ; i++)
	{
		rcData[i]=1500;
	}
	//open serialport
	serial = open ( SERIAL , O_RDWR | O_NOCTTY | O_NDELAY );
	
	struct termios options;

    //read current settings
	tcgetattr(serial, &options);
	//raw mode -> turn all the special features off
	//+ use 8N1
	cfmakeraw(&options);
	//set baudrate
	cfsetispeed(&options, BAUD);
	cfsetospeed(&options, BAUD);
	//enable receiving
	options.c_cflag |=CREAD;
	//minimum number of chars to read
	options.c_cc[VMIN]=22;
	//timeout reading
	options.c_cc[VTIME]=2;
	//no flowcontrol
	#ifdef CNEW_RTSCTS
		options.c_cflag &= ~CNEW_RTSCTS;
	#endif
	//write options
	tcsetattr(serial, TCSANOW, &options);
    
    
    
	int done = 1;
	if (SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_VIDEO) != 0)
	{
		printf ("Fehler: %s\n", SDL_GetError ());
		return EXIT_FAILURE;
	}
	atexit (SDL_Quit);
	Joystick_Info ();
	js = SDL_JoystickOpen (JOYSTICK_N);
	if (js == NULL) 
	{
		printf("Couldn't open desired Joystick:%s\n",SDL_GetError());
		done=0;
	}
	
	int res;
	char buf[255];
	while( done ) 
	{
		done = eventloop_joystick ();
		if(done>1)
			sendRC();
		
		
		res = read(serial,buf,255);
		if(res>0)
		{
			
			for(i=0;i<res;i++)
					printf("%c",buf[i]);
			
			printf("\n");
			
			for(i=0;i<res;i++)
					printf("%u|",buf[i]);
					
			printf("\n");
		}
		
	}
	  
  SDL_JoystickClose (js);
  close (serial);
  return EXIT_SUCCESS;
}
