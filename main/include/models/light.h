#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <map>

enum StripType : uint8_t {
  NeoPixel_GRB = 0,
  NeoPixel_GRBW,
  NeoPixel_RGB,
  NeoPixel_RGBW,
  DotStar_BGR,
  DotStar_LBGR,
  DotStar_GRB,
  DotStar_LGRB
};

struct LightChannel {
  uint8_t channel;
  uint16_t leds;
  LEDType type;
};

struct LightSection {
  uint8_t channel;
  uint16_t start;
  uint16_t end;
};

struct LightRegion {
  std::string name;
  std::vector<LightSection> sections;
  uint32_t count;
  std::vector<uint16_t> breaks;
};

struct LightsConfig {
  std::map<std::string, LightRegion> regions;
  std::map<uint8_t, LightChannel> channels;
};

enum LightEffect : uint8_t {
  Off = 0x00,
  Static,
  Blink,
  ColorWipe,
  Breathe,
  Fade,
  Scan,
  Rainbow,
  RainbowCycle,
  ColorChase,
  TheaterChase,
  TheaterChaseRainbow,
  Twinkle,
  Sparkle,
  Alternate
};

enum LightCommand {
  NoCommand = 0x00,
  LightsOff,
  LightsReset,
  LightsBrakeNormal,
  LightsBrakeActive,
  LightsHeadlightNormal,
  LightsHeadlightBright,
  LightsTurnCenter,
  LightsTurnLeft,
  LightsTurnRight,
  LightsTurnHazard
};

struct LightCommands {
  LightCommand brakeCommand;
  LightCommand turnCommand;
  LightCommand headlightCommand;
};

struct LightingParameters {
  std::string region;
  LightEffect effect;
  uint8_t layer;
  Color first;
  Color second;
  Color third;
  uint32_t duration;
};

struct RenderStep {
  unsigned long step;
  unsigned long next;
  uint32_t tempPixel;
};

inline bool operator< (const LightingParameters& lhs, const LightingParameters& rhs){ return lhs.layer < rhs.layer; }
inline bool operator> (const LightingParameters& lhs, const LightingParameters& rhs){ return lhs.layer > rhs.layer; }
inline bool operator<=(const LightingParameters& lhs, const LightingParameters& rhs){ return lhs.layer <= rhs.layer; }
inline bool operator>=(const LightingParameters& lhs, const LightingParameters& rhs){ return lhs.layer >= rhs.layer; }
inline bool operator==(const LightingParameters& lhs, const LightingParameters& rhs){ return lhs.layer == rhs.layer; }
inline bool operator!=(const LightingParameters& lhs, const LightingParameters& rhs){ return lhs.layer != rhs.layer; }