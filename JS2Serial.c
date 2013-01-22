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
FILE* serialFile;

static SDL_Joystick *js;
static void Joystick_Info (void) 
{
  int num_js, i;
  num_js = SDL_NumJoysticks ();
  printf ("Es wurde(n) %d Joystick(s) auf dem System gefunden\n",
     num_js);
  if (num_js == 0) {
    printf ("Es wurde kein Joystick gefunden\n");
    return;
  }
  /* Informationen zum Joystick */
  for (i = 0; i < num_js; i++) {
    js = SDL_JoystickOpen (i);
    if (js == NULL) {
      printf ("Kann Joystick %d nicht öffnen\n", i);
    } 
    else {
      printf ("Joystick %d\n", i);
      printf ("\tName:       %s\n", SDL_JoystickName(i));
      printf ("\tAxen:       %i\n", SDL_JoystickNumAxes(js));
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
				printf ("ESCAPE für Ende betätigt\n");
				return 0;
			}
			break;
		case SDL_JOYAXISMOTION:
			//lokal
			printf ("Joystick %d, Achse %d bewegt nach %d\n",
			 event.jaxis.which, event.jaxis.axis, event.jaxis.value);
			 //über serielle Verbindung
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
	uint8_t outputbuffer[22];
	//Header
	outputbuffer[0]='$';
	outputbuffer[1]='M';
	outputbuffer[2]='<';
	//Size
	outputbuffer[3]=16;
	//Message type
	outputbuffer[4]=200;
	
	for(i=5; i<21 ; i++)
	{
		//low byte
		outputbuffer[i]=(uint8_t)rcData[i]&0xFF;
		checksum^=outputbuffer[i];
		i++;
		//high byte
		outputbuffer[i]=(uint8_t)(rcData[i]>>8);
		checksum^=outputbuffer[i];
	}
	outputbuffer[21]=checksum;
	
	fwrite(outputbuffer,sizeof(uint8_t),22,serialFile);
	fprintf(serialFile, "\n");
	tcflush(serial, TCIFLUSH);
	/*fprintf(serialFile,"$M< 16 200 %i %i %i %i %i %i %i %i %i \n",
		rcData[0], rcData[1], rcData[2], rcData[3], rcData[4], rcData[5], rcData[6], rcData[7], checksum);*/
}


int main (void) 
{
	//vorfüllen mit Mittelstellung
	uint8_t i;
	for(i=0; i<8 ; i++)
	{
		rcData[i]=1500;
	}
	//seriellen Bluetooth Anschluss als Datei öffnen
	serialFile = fopen(SERIAL, "a");
	serial = open ( SERIAL , O_RDWR | O_NOCTTY | O_NDELAY );
	
	struct termios options;

    //aktuelle Porteinstellungen auslesen
    tcgetattr(serial, &options);
    //baud rate setzen
    cfsetispeed(&options, BAUD);
    cfsetospeed(&options, BAUD);
    // Enable the receiver and set local mode...
    options.c_cflag |= (CLOCAL | CREAD);
    //8N1 "keine Parität"
    options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	//Empfang erlauben
	options.c_cflag |=CREAD;
	//keine Flusssteuerung
	#ifdef CNEW_RTSCTS
		options.c_cflag &= ~CNEW_RTSCTS;
	#endif
    //Optionen schreiben
    tcsetattr(serial, TCSANOW, &options);
    
    
    
	int done = 1;
	if (SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_VIDEO) != 0)
	{
		printf ("Fehler: %s\n", SDL_GetError ());
		return 1;
	}
	atexit (SDL_Quit);
	Joystick_Info ();
	js = SDL_JoystickOpen (JOYSTICK_N);
	if (js == NULL) 
	{
		printf("Konnte Joystick nicht öffnen:%s\n",SDL_GetError());
	}
	
	int res;
	char buf[255];
	while( done ) 
	{
		done = eventloop_joystick ();
		if(done>1)
			sendRC();
			
		res = read(serial,buf,255);
		if(res!=0)
		{
			
			for(i=0;i<res;i++)
			{
				if(i<3)
					printf("%c",buf[i]);
				else
					printf("%u|",buf[i]);
			}
			printf("\n");
		}	
		
	}
	  
  SDL_JoystickClose (js);
  //"Datei" des seriellenPorts schließen
  fclose(serialFile);
  close (serial);
  return EXIT_SUCCESS;
}
