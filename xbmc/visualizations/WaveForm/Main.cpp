/*
*      Copyright (C) 2008-2013 Team XBMC
*      http://xbmc.org
*
*  This Program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This Program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with XBMC; see the file COPYING.  If not, see
*  <http://www.gnu.org/licenses/>.
*
*/

// Waveform.vis
// A simple visualisation example by MrC

#include "addons/include/xbmc_vis_dll.h"
#include <stdio.h>
#ifdef HAS_SDL_OPENGL
#include <GL/glew.h>
#else
#ifdef _WIN32
#include <D3D9.h>
#endif
#endif

//th
#include <curl/curl.h>
#include "fft.h"
#include <string>
#include <math.h>
#include <sstream>
//

char g_visName[512];
#ifndef HAS_SDL_OPENGL
LPDIRECT3DDEVICE9 g_device;
#else
void* g_device;
#endif
float g_fWaveform[2][512];

#ifdef HAS_SDL_OPENGL
typedef struct {
  int X;
  int Y;
  int Width;
  int Height;
  int MinZ;
  int MaxZ;
} D3DVIEWPORT9;
typedef unsigned long D3DCOLOR;
#endif

D3DVIEWPORT9  g_viewport;

struct Vertex_t
{
  float x, y, z;
  D3DCOLOR  col;
};

#ifndef HAS_SDL_OPENGL
#define VERTEX_FORMAT     (D3DFVF_XYZ | D3DFVF_DIFFUSE)
#endif

//th
#define BUFFERSIZE 1024
#define NUM_FREQUENCIES (512)
CURL *curl;
CURLcode res;

namespace
{
  // User config settings
  //UserSettings g_Settings;

  FFT g_fftobj;

#ifdef _WIN32
  FLOAT fSecsPerTick;
  LARGE_INTEGER qwTime, qwLastTime, qwLightTime, qwElapsedTime, qwAppTime, qwElapsedAppTime;
#endif
  float fTime, fElapsedTime, fAppTime, fElapsedAppTime, fUpdateTime, fLastTime, fLightTime;
  int iFrames = 0;
  float fFPS = 0;
}

struct SoundData
{
  float   imm[2][3];                // bass, mids, treble, no damping, for each channel (long-term average is 1)
  float   avg[2][3];               // bass, mids, treble, some damping, for each channel (long-term average is 1)
  float   med_avg[2][3];          // bass, mids, treble, more damping, for each channel (long-term average is 1)
  //    float   long_avg[2][3];        // bass, mids, treble, heavy damping, for each channel (long-term average is 1)
  float   fWaveform[2][576];             // Not all 576 are valid! - only NUM_WAVEFORM_SAMPLES samples are valid for each channel (note: NUM_WAVEFORM_SAMPLES is declared in shell_defines.h)
  float   fSpectrum[2][NUM_FREQUENCIES]; // NUM_FREQUENCIES samples for each channel (note: NUM_FREQUENCIES is declared in shell_defines.h)

  float specImm[32];
  float specAvg[32];
  float specMedAvg[32];

  float bigSpecImm[512];
  float leftBigSpecAvg[512];
  float rightBigSpecAvg[512];
};

SoundData g_sound;
float g_bass, g_bassLast;
float g_treble, g_trebleLast;
float g_middle, g_middleLast;
float g_timePass;
bool g_finished, g_beat;

//todo: get bridge IP from settings or from https://www.meethue.com/api/nupnp
std::string strHueBridgeIPAddress = "192.168.10.6";
std::string strHost = "Host: " + strHueBridgeIPAddress;
std::string strURLRegistration = "http://" + strHueBridgeIPAddress + "/api";
std::string strURLLight1 = "http://" + strHueBridgeIPAddress + "/api/KodiVisWave/lights/1/state";
std::string strURLLight2 = "http://" + strHueBridgeIPAddress + "/api/KodiVisWave/lights/2/state";
int lastHue;
int initialHue;
int targetHue;
int maxBri = 75;

#ifndef _WIN32
struct timespec systemClock;
#endif



void HTTP_POST(int bri, int sat, int transitionTime, bool on, bool off)
{
  std::string strJson;

  if (on) //turn on
    strJson = "{\"on\":true}";
  else if (off) //turn light off
    strJson = "{\"on\":false}";
  else if (sat > 0) //change saturation
  {
    std::ostringstream oss;
    oss << "{\"bri\":" << bri << ",\"hue\":" << lastHue <<
      ",\"sat\":" << sat << ",\"transitiontime\":"
      << transitionTime << "}";
    strJson = oss.str();
  }
  else //change lights
  {
    std::ostringstream oss;
    oss << "{\"bri\":" << bri << ",\"hue\":" << lastHue <<
      ",\"transitiontime\":" << transitionTime << "}";
    strJson = oss.str();
  }
  /*
  else if (sat > 0) //change saturation
    strJson = "{\"bri\":" + std::to_string(bri) +
      ",\"hue\":" + std::to_string(lastHue) + ",\"sat\":" + std::to_string(sat) + ",\"transitiontime\":" + std::to_string(transitionTime) + "}";
  else //change lights
    strJson = "{\"bri\":" + std::to_string(bri) +
      ",\"hue\":" + std::to_string(lastHue) + ",\"transitiontime\":" + std::to_string(transitionTime) + "}";
  */

  //request2 << "Content-Type: application/json" << endl;
  //struct curl_slist *headers = NULL;
  //curl = curl_easy_init();
  if (curl) {
    //append headers

    // Now specify we want to PUT data, but not using a file, so it has o be a CUSTOMREQUEST
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    //curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strJson.c_str());

    // Set the URL that is about to receive our POST. 
    curl_easy_setopt(curl, CURLOPT_URL, strURLLight1.c_str());
    // Perform the request, res will get the return code
    res = curl_easy_perform(curl);

    // Set the URL that is about to receive our POST.
    curl_easy_setopt(curl, CURLOPT_URL, strURLLight2.c_str());
    // Perform the request, res will get the return code
    res = curl_easy_perform(curl);

    // always cleanup 
  }
}
//

void TurnLightsOn()
{
  HTTP_POST(0, 0, 0, true, false);
}

void UpdateLights(int bri, int sat, int transitionTime)
{
  HTTP_POST(bri, sat, transitionTime, false, false);
}

void FastBeatLights(int bri)
{
  int transitionTime = int(10 / (60 / 60)); // transitionTime is in deciseconds
  //transition the color immediately
  UpdateLights(bri, 0, 0);
  //fade brightness
  UpdateLights(5, 0, transitionTime); //fade
}

void SlowBeatLights(int bri)
{
  int transitionTime = int(10 / (60 / 60)); // transitionTime is in deciseconds
  //transition the color immediately
  UpdateLights(bri, 0, 2);
  //fade brightness
  UpdateLights(5, 0, transitionTime - 2); //fade
}

void CycleHue(int huePoints)
{
  int hueGap;
  if ((lastHue - targetHue) > 0) hueGap = lastHue - targetHue;
  else hueGap = (lastHue - targetHue) * -1;
  if (hueGap > huePoints)
  {
    if (lastHue > targetHue) lastHue = lastHue - huePoints;
    else lastHue = lastHue + huePoints;
  }
  else
  {
    lastHue = targetHue;
    targetHue = initialHue;
    initialHue = lastHue;
  }
}

void CycleLights(int bri)
{
  //this is called once per second if no beats are detected
  CycleHue(3000);
  UpdateLights(bri, 0, 10);
}

//taken from Vortex
float AdjustRateToFPS(float per_frame_decay_rate_at_fps1, float fps1, float actual_fps)
{
  // returns the equivalent per-frame decay rate at actual_fps

  // basically, do all your testing at fps1 and get a good decay rate;
  // then, in the real application, adjust that rate by the actual fps each time you use it.

  float per_second_decay_rate_at_fps1 = powf(per_frame_decay_rate_at_fps1, fps1);
  float per_frame_decay_rate_at_fps2 = powf(per_second_decay_rate_at_fps1, 1.0f / actual_fps);

  return per_frame_decay_rate_at_fps2;
}

//taken from Vortex
void AnalyzeSound()
{
  // Some bits of this were pinched from Milkdrop...
  int m_fps = 60;

  // sum (left channel) spectrum up into 3 bands
  // [note: the new ranges do it so that the 3 bands are equally spaced, pitch-wise]
  float min_freq = 200.0f;
  float max_freq = 11025.0f;
  float net_octaves = (logf(max_freq / min_freq) / logf(2.0f));     // 5.7846348455575205777914165223593
  float octaves_per_band = net_octaves / 3.0f;                    // 1.9282116151858401925971388407864
  float mult = powf(2.0f, octaves_per_band); // each band's highest freq. divided by its lowest freq.; 3.805831305510122517035102576162
  // [to verify: min_freq * mult * mult * mult should equal max_freq.]
  //    for (int ch=0; ch<2; ch++)
  {
    for (int i = 0; i<3; i++)
    {
      // old guesswork code for this:
      //   float exp = 2.1f;
      //   int start = (int)(NUM_FREQUENCIES*0.5f*powf(i/3.0f, exp));
      //   int end   = (int)(NUM_FREQUENCIES*0.5f*powf((i+1)/3.0f, exp));
      // results:
      //          old range:      new range (ideal):
      //   bass:  0-1097          200-761
      //   mids:  1097-4705       761-2897
      //   treb:  4705-11025      2897-11025
      int start = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)i) / 11025.0f);
      int end = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)i + 1) / 11025.0f);
      if (start < 0) start = 0;
      if (end > NUM_FREQUENCIES) end = NUM_FREQUENCIES;

      g_sound.imm[0][i] = 0;
      for (int j = start; j<end; j++)
      {
        g_sound.imm[0][i] += g_sound.fSpectrum[0][j];
        g_sound.imm[0][i] += g_sound.fSpectrum[1][j];
      }
      g_sound.imm[0][i] /= (float)(end - start) * 2;
    }
  }



  // multiply by long-term, empirically-determined inverse averages:
  // (for a trial of 244 songs, 10 seconds each, somewhere in the 2nd or 3rd minute,
  //  the average levels were: 0.326781557	0.38087377	0.199888934
  for (int ch = 0; ch<2; ch++)
  {
    g_sound.imm[ch][0] /= 0.326781557f;//0.270f;   
    g_sound.imm[ch][1] /= 0.380873770f;//0.343f;   
    g_sound.imm[ch][2] /= 0.199888934f;//0.295f;   
  }

  // do temporal blending to create attenuated and super-attenuated versions
  for (int ch = 0; ch<2; ch++)
  {
    for (int i = 0; i<3; i++)
    {
      // g_sound.avg[i]
      {
        float avg_mix;
        if (g_sound.imm[ch][i] > g_sound.avg[ch][i])
          avg_mix = AdjustRateToFPS(0.2f, 14.0f, (float)m_fps);
        else
          avg_mix = AdjustRateToFPS(0.5f, 14.0f, (float)m_fps);
        //                if (g_sound.imm[ch][i] > g_sound.avg[ch][i])
        //                  avg_mix = 0.5f;
        //                else 
        //                  avg_mix = 0.8f;
        g_sound.avg[ch][i] = g_sound.avg[ch][i] * avg_mix + g_sound.imm[ch][i] * (1 - avg_mix);
      }

      {
        float med_mix = 0.91f;//0.800f + 0.11f*powf(t, 0.4f);    // primarily used for velocity_damping
        float long_mix = 0.96f;//0.800f + 0.16f*powf(t, 0.2f);    // primarily used for smoke plumes
        med_mix = AdjustRateToFPS(med_mix, 14.0f, (float)m_fps);
        long_mix = AdjustRateToFPS(long_mix, 14.0f, (float)m_fps);
        g_sound.med_avg[ch][i] = g_sound.med_avg[ch][i] * (med_mix)+g_sound.imm[ch][i] * (1 - med_mix);
        //                g_sound.long_avg[ch][i] = g_sound.long_avg[ch][i]*(long_mix) + g_sound.imm[ch][i]*(1-long_mix);
      }
    }
  }

  float newBass = ((g_sound.avg[0][0] - g_sound.med_avg[0][0]) / g_sound.med_avg[0][0]) * 2;
  float newMiddle = ((g_sound.avg[0][1] - g_sound.med_avg[0][1]) / g_sound.med_avg[0][1]) * 2;
  float newTreble = ((g_sound.avg[0][2] - g_sound.med_avg[0][2]) / g_sound.med_avg[0][2]) * 2;

#ifdef _WIN32
  newBass = max(min(newBass, 1.0f), -1.0f);
  newMiddle = max(min(newMiddle, 1.0f), -1.0f);
  newTreble = max(min(newTreble, 1.0f), -1.0f);
#else
  newBass = std::max(std::min(newBass, 1.0f), -1.0f);
  newMiddle = std::max(std::min(newMiddle, 1.0f), -1.0f);
  newTreble = std::max(std::min(newTreble, 1.0f), -1.0f);
#endif

  g_bassLast = g_bass;
  g_middleLast = g_middle;

  float avg_mix;
  if (newTreble > g_treble)
    avg_mix = 0.5f;
  else
    avg_mix = 0.5f;
  
  //crazy NaN's in linux
  if (g_bass != g_bass) g_bass = 0;
  if (g_middle != g_middle) g_middle = 0; 
  if (g_treble != g_treble) g_treble = 0;

  g_bass = g_bass*avg_mix + newBass*(1 - avg_mix);
  g_middle = g_middle*avg_mix + newMiddle*(1 - avg_mix);
  //g_treble = g_treble*avg_mix + newTreble*(1 - avg_mix);

#ifdef _WIN32
  g_bass = max(min(g_bass, 1.0f), -1.0f);
  g_middle = max(min(g_middle, 1.0f), -1.0f);
  //g_treble = max(min(g_treble, 1.0f), -1.0f);
#else
  g_bass = std::max(std::min(g_bass, 1.0f), -1.0f);
  g_middle = std::max(std::min(g_middle, 1.0f), -1.0f);
  //g_treble = std::max(std::min(g_treble, 1.0f), -1.0f);
#endif

  if (g_middle < 0) g_middle = g_middle * -1.0f;
  if ((g_middle - g_middleLast) > 0.325f && !g_beat)
  {
    //beat?
    g_beat = true;
    FastBeatLights(maxBri);

    CycleHue(1500);
    //changed lights
    fLightTime = fAppTime;
  }
  else g_beat = false;


}

VOID InitTime()
{
#ifdef _WIN32
  // Get the frequency of the timer
  LARGE_INTEGER qwTicksPerSec;
  QueryPerformanceFrequency(&qwTicksPerSec);
  fSecsPerTick = 1.0f / (FLOAT)qwTicksPerSec.QuadPart;

  // Save the start time
  QueryPerformanceCounter(&qwTime);
  qwLastTime.QuadPart = qwTime.QuadPart;
  qwLightTime.QuadPart = qwTime.QuadPart;

  qwAppTime.QuadPart = 0;
  qwElapsedTime.QuadPart = 0;
  qwElapsedAppTime.QuadPart = 0;
  srand(qwTime.QuadPart);
#else
  // Save the start time
  clock_gettime(CLOCK_MONOTONIC, &systemClock);
  fTime = ((float)systemClock.tv_nsec / 1000000000.0) + (float)systemClock.tv_sec;
#endif

  fAppTime = 0;
  fElapsedTime = 0;
  fElapsedAppTime = 0;
  fLastTime = 0;
  fLightTime = 0;
  fUpdateTime = 0;

}

VOID UpdateTime()
{
#ifdef _WIN32
  QueryPerformanceCounter(&qwTime);
  qwElapsedTime.QuadPart = qwTime.QuadPart - qwLastTime.QuadPart;
  qwLastTime.QuadPart = qwTime.QuadPart;
  qwLightTime.QuadPart = qwTime.QuadPart;
  qwElapsedAppTime.QuadPart = qwElapsedTime.QuadPart;
  qwAppTime.QuadPart += qwElapsedAppTime.QuadPart;

  // Store the current time values as floating point
  fTime = fSecsPerTick * ((FLOAT)(qwTime.QuadPart));
  fElapsedTime = fSecsPerTick * ((FLOAT)(qwElapsedTime.QuadPart));
  fAppTime = fSecsPerTick * ((FLOAT)(qwAppTime.QuadPart));
  fElapsedAppTime = fSecsPerTick * ((FLOAT)(qwElapsedAppTime.QuadPart));
#else
  clock_gettime(CLOCK_MONOTONIC, &systemClock);
  fTime = ((float)systemClock.tv_nsec / 1000000000.0) + (float)systemClock.tv_sec;
  fElapsedTime = fTime - fLastTime;
  fLastTime = fTime;
  fAppTime += fElapsedTime;
#endif

  // Keep track of the frame count
  iFrames++;

  //fBeatTime = 60.0f / (FLOAT)(bpm); //skip every other beat

  // If beats aren't doing anything then cycle colors nicely
  if (fAppTime - fLightTime > 1.5f)
  {
    CycleLights(maxBri); 
    fLightTime = fAppTime;
  }
  

  // Update the scene stats once per second
  if (fAppTime - fUpdateTime > 1.0f)
  {
    fFPS = (float)(iFrames / (fAppTime - fLastTime));
    fUpdateTime = fAppTime;
    iFrames = 0;
  }
}



//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create(void* hdl, void* props)
{

  if (!props)
    return ADDON_STATUS_UNKNOWN;

  VIS_PROPS* visProps = (VIS_PROPS*)props;

#ifndef HAS_SDL_OPENGL  
  g_device = (LPDIRECT3DDEVICE9)visProps->device;
#else
  g_device = visProps->device;
#endif
  g_viewport.X = visProps->x;
  g_viewport.Y = visProps->y;
  g_viewport.Width = visProps->width;
  g_viewport.Height = visProps->height;
  g_viewport.MinZ = 0;
  g_viewport.MaxZ = 1;

  //set Hue registration command
  std::string json = "{\"devicetype\":\"Kodi\",\"username\":\"KodiVisWave\"}";

  //struct curl_slist *headers = NULL;
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    //curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

    // Set the URL that is about to receive our POST.
    curl_easy_setopt(curl, CURLOPT_URL, strURLRegistration.c_str());

    // Perform the request, res will get the return code
    res = curl_easy_perform(curl);

    // always cleanup

  }


  lastHue = 65280;
  initialHue = lastHue;
  targetHue = 56100;

  //turn the lights on
  TurnLightsOn();
  UpdateLights(maxBri, 255, 30);

  //initialize the beat detection
  InitTime();
  g_fftobj.Init(576, NUM_FREQUENCIES);
  //

  return ADDON_STATUS_OK;
  //return ADDON_STATUS_NEED_SAVEDSETTINGS;
}

//-- Start --------------------------------------------------------------------
// Called when a new soundtrack is played
//-----------------------------------------------------------------------------
extern "C" void Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName)
{
  
}

//-- Audiodata ----------------------------------------------------------------
// Called by XBMC to pass new audio data to the vis
//-----------------------------------------------------------------------------
extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
{
  int ipos = 0;
  while (ipos < 512)
  {
    for (int i = 0; i < iAudioDataLength; i += 2)
    {
      g_fWaveform[0][ipos] = pAudioData[i]; // left channel
      g_fWaveform[1][ipos] = pAudioData[i + 1]; // right channel
      ipos++;
      if (ipos >= 512) break;
    }
  }

  //taken from Vortex
  float tempWave[2][576];

  int iPos = 0;
  int iOld = 0;
  //const float SCALE = (1.0f / 32768.0f ) * 255.0f;
  while (iPos < 576)
  {
    for (int i = 0; i < iAudioDataLength; i += 2)
    {
      g_sound.fWaveform[0][iPos] = float((pAudioData[i] / 32768.0f) * 255.0f);
      g_sound.fWaveform[1][iPos] = float((pAudioData[i + 1] / 32768.0f) * 255.0f);

      // damp the input into the FFT a bit, to reduce high-frequency noise:
      tempWave[0][iPos] = 0.5f * (g_sound.fWaveform[0][iPos] + g_sound.fWaveform[0][iOld]);
      tempWave[1][iPos] = 0.5f * (g_sound.fWaveform[1][iPos] + g_sound.fWaveform[1][iOld]);
      iOld = iPos;
      iPos++;
      if (iPos >= 576)
        break;
    }
  }

  g_fftobj.time_to_frequency_domain(tempWave[0], g_sound.fSpectrum[0]);
  g_fftobj.time_to_frequency_domain(tempWave[1], g_sound.fSpectrum[1]);
}


//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
extern "C" void Render()
{
  Vertex_t  verts[512];

#ifndef HAS_SDL_OPENGL
  g_device->SetFVF(VERTEX_FORMAT);
  g_device->SetPixelShader(NULL);
#endif

  // Left channel
#ifdef HAS_SDL_OPENGL
  GLenum errcode;
  glColor3f(1.0, 1.0, 1.0);
  glDisable(GL_BLEND);
  glPushMatrix();
  glTranslatef(0, 0, -1.0);
  glBegin(GL_LINE_STRIP);
#endif
  for (int i = 0; i < 256; i++)
  {
    verts[i].col = 0xffffffff;
    verts[i].x = g_viewport.X + ((i / 255.0f) * g_viewport.Width);
    verts[i].y = g_viewport.Y + g_viewport.Height * 0.33f + (g_fWaveform[0][i] * g_viewport.Height * 0.15f);
    verts[i].z = 1.0;
#ifdef HAS_SDL_OPENGL
    glVertex2f(verts[i].x, verts[i].y);
#endif
  }

#ifdef HAS_SDL_OPENGL
  glEnd();
  if ((errcode = glGetError()) != GL_NO_ERROR) {
    printf("Houston, we have a GL problem: %s\n", gluErrorString(errcode));
  }
#elif !defined(HAS_SDL_OPENGL)
  g_device->DrawPrimitiveUP(D3DPT_LINESTRIP, 255, verts, sizeof(Vertex_t));
#endif

  // Right channel
#ifdef HAS_SDL_OPENGL
  glBegin(GL_LINE_STRIP);
#endif
  for (int i = 0; i < 256; i++)
  {
    verts[i].col = 0xffffffff;
    verts[i].x = g_viewport.X + ((i / 255.0f) * g_viewport.Width);
    verts[i].y = g_viewport.Y + g_viewport.Height * 0.66f + (g_fWaveform[1][i] * g_viewport.Height * 0.15f);
    verts[i].z = 1.0;
#ifdef HAS_SDL_OPENGL
    glVertex2f(verts[i].x, verts[i].y);
#endif
  }

#ifdef HAS_SDL_OPENGL
  glEnd();
  glEnable(GL_BLEND);
  glPopMatrix();
  if ((errcode = glGetError()) != GL_NO_ERROR) {
    printf("Houston, we have a GL problem: %s\n", gluErrorString(errcode));
  }
#elif !defined(HAS_SDL_OPENGL)
  g_device->DrawPrimitiveUP(D3DPT_LINESTRIP, 255, verts, sizeof(Vertex_t));
#endif

  //get some interesting numbers to play with
  UpdateTime();
  //g_timePass = 1.0f / 60.f;//fElapsedAppTime;
  g_timePass = fElapsedAppTime;
  AnalyzeSound();

}

//-- GetInfo ------------------------------------------------------------------
// Tell XBMC our requirements
//-----------------------------------------------------------------------------
extern "C" void GetInfo(VIS_INFO* pInfo)
{
  pInfo->bWantsFreq = false;
  pInfo->iSyncDelay = 0;
}

//-- OnAction -----------------------------------------------------------------
// Handle XBMC actions such as next preset, lock preset, album art changed etc
//-----------------------------------------------------------------------------
extern "C" bool OnAction(long flags, const void *param)
{
  bool ret = false;
  return ret;
}

//-- GetPresets ---------------------------------------------------------------
// Return a list of presets to XBMC for display
//-----------------------------------------------------------------------------
extern "C" unsigned int GetPresets(char ***presets)
{
  return 0;
}

//-- GetPreset ----------------------------------------------------------------
// Return the index of the current playing preset
//-----------------------------------------------------------------------------
extern "C" unsigned GetPreset()
{
  return 0;
}

//-- IsLocked -----------------------------------------------------------------
// Returns true if this add-on use settings
//-----------------------------------------------------------------------------
extern "C" bool IsLocked()
{
  return false;
}

//-- GetSubModules ------------------------------------------------------------
// Return any sub modules supported by this vis
//-----------------------------------------------------------------------------
extern "C" unsigned int GetSubModules(char ***names)
{
  return 0; // this vis supports 0 sub modules
}

//-- Stop ---------------------------------------------------------------------
// This dll must stop all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Stop()
{
}

//-- Detroy -------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Destroy()
{
  lastHue = 57000;
  UpdateLights(maxBri, 255, 20);
  g_fftobj.CleanUp();
  // always cleanup 
  curl_easy_cleanup(curl);
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" bool ADDON_HasSettings()
{
  return true;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" unsigned int ADDON_GetSettings(ADDON_StructSetting*** sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

extern "C" void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_SetSetting(const char *id, const void* value)
{
  if (!id || !value)
    return ADDON_STATUS_UNKNOWN;

  //UserSettings& userSettings = g_Vortex->GetUserSettings();

  if (strcmpi(id, "HueBridgeIP") == 0)
  {
    std::stringstream strm;
    strm << value;
    strHueBridgeIPAddress = strm.str();
  }
  else if (strcmpi(id, "MaxBri") == 0)
  {
    maxBri = (int)value;
  }
  else if (strcmpi(id, "HueRangeUpper") == 0)
  {
    initialHue = (int)value;
  }
  else if (strcmpi(id, "HueRangeLower") == 0)
  {
    targetHue = (int)value;
  }
  else
    return ADDON_STATUS_UNKNOWN;
  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

