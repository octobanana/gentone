/*
                                    88888888
                                  888888888888
                                 88888888888888
                                8888888888888888
                               888888888888888888
                              888888  8888  888888
                              88888    88    88888
                              888888  8888  888888
                              88888888888888888888
                              88888888888888888888
                             8888888888888888888888
                          8888888888888888888888888888
                        88888888888888888888888888888888
                              88888888888888888888
                            888888888888888888888888
                           888888  8888888888  888888
                           888     8888  8888     888
                                   888    888

                                   OCTOBANANA

Licensed under the MIT License

Copyright (c) 2020 Brett Robinson <https://octobanana.com/>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "info.hh"
#include "ob/parg.hh"
#include "ob/term.hh"
#include "ob/prism.hh"
#include "ob/string.hh"

#include <SFML/Audio.hpp>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <csignal>

#include <chrono>
#include <thread>
#include <limits>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <type_traits>

// #define dbg(x) std::cerr << "DBG> "#x": " << (x) << "\n"

using Parg = OB::Parg;
namespace Term = OB::Term;
namespace iom = OB::Term::iomanip;
namespace aec = OB::Term::ANSI_Escape_Codes;

bool is_term {false};
bool use_color {false};
std::size_t cursor_y {0};

std::vector<std::string> const channel_str {
  "unknown",
  "mono",
  "stereo",
  "left",
  "right",
};

struct Channel {
  enum Type {
    Mono = 1,
    Stereo = 2,
    Left = 3,
    Right = 4,
  };
};

struct Track {
  sf::SoundBuffer buf;
  sf::Sound sound;
};

struct Wave {
  int num_channels {0};
  int sample_rate {0};
  int num_samples {0};
  std::vector<short> samples;
};

struct Data {
  std::string graphic;
  double a4 {0};
  double sos {0};
  std::string note;
  double freq {0};
  double size {0};
  std::string wave;
  int rate {0};
  double ampl {0};
  int chan {0};
  double time {0};
  bool loop {false};
};

template <typename T = std::chrono::milliseconds>
void sleep(T const& duration) {
  std::this_thread::sleep_for(duration);
}

template<typename T>
static typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type almost_equal(T x, T y, int z = 2) {
  return std::fabs(x - y) <= std::numeric_limits<T>::epsilon() * std::fabs(x + y) * z || std::fabs(x - y) < std::numeric_limits<T>::min();
}


template<typename T>
T scale(T const val, T const in_min, T const in_max, T const out_min, T const out_max) {
  assert(in_min <= in_max);
  assert(out_min <= out_max);
  return (out_min + (out_max - out_min) * ((val - in_min) / (in_max - in_min)));
}

void signal_handler(int signal);
std::string term_fg(OB::Prism::RGBA rgba);
std::string term_bg(OB::Prism::RGBA rgba);
void smooth_samples(Wave& wave);
std::string freq_to_note(double const freq, double const a4 = 440.0);
double note_to_freq(std::string const& note, double const a4 = 440.0);
Wave make_wave(Data const& data);
Track make_track(Wave const& wave, bool const loop);
bool is_playing(Track const& track);
void draw_wave(Wave const& wave, Data const& data, Track const* track = nullptr);
void save_to_file(Wave const& wave, std::string const& output);
Data make_data(Parg& pg);
void print_data(Data const& data);

void signal_handler(int signal) {
  std::cout << aec::clear;
  if (cursor_y > 0) {
    std::cout << aec::cursor_set(1, cursor_y);
  }
  std::cout << "\n" << aec::cursor_show << std::flush;
  std::exit(1);
}

std::string term_fg(OB::Prism::RGBA rgba) {
  std::string s;
  s += "\x1b[38;2;";
  s += std::to_string(static_cast<int>(rgba.r()));
  s += ";";
  s += std::to_string(static_cast<int>(rgba.g()));
  s += ";";
  s += std::to_string(static_cast<int>(rgba.b()));
  s += "m";
  return s;
}

std::string term_bg(OB::Prism::RGBA rgba) {
  std::string s;
  s += "\x1b[48;2;";
  s += std::to_string(static_cast<int>(rgba.r()));
  s += ";";
  s += std::to_string(static_cast<int>(rgba.g()));
  s += ";";
  s += std::to_string(static_cast<int>(rgba.b()));
  s += "m";
  return s;
}

void smooth_samples(Wave& wave) {
  for (auto it = wave.samples.rbegin(); it != wave.samples.rend(); ++it) {
    if (*it <= 0) {
      auto dist = std::distance(wave.samples.rbegin(), std::prev(it));
      wave.num_samples -= dist;
      wave.samples.erase(it.base(), std::rbegin(wave.samples).base());
      break;
    }
  }
}

std::string freq_to_note(double const freq, double const a4) {
  if (freq <= 0) {throw std::runtime_error("invalid freq '" + std::to_string(freq) + "'");}
  std::unordered_map<int, std::string> const c_offset {
    { 0, "C"}, { 1, "C#"},
    { 2, "D"}, { 3, "D#"},
    { 4, "E"},
    { 5, "F"}, { 6, "F#"},
    { 7, "G"}, { 8, "G#"},
    { 9, "A"}, {10, "A#"},
    {11, "B"},
  };
  int const semitones {static_cast<int>(std::round(std::log(freq / a4) / std::log(std::pow(2.0, 1.0 / 12.0))) + 57)};
  std::size_t const offset {static_cast<std::size_t>(semitones % 12)};
  int const octave {semitones / 12};
  return c_offset.at(offset) + std::to_string(octave);
}

double note_to_freq(std::string const& note, double const a4) {
  auto m = OB::String::match(OB::String::lowercase(note), std::regex("^([a-g]{1}(?:[b#]{0,1}?))([0-9]+)$"));
  if (!m) {throw std::runtime_error("invalid note '" + note + "'");}
  std::unordered_map<std::string, int> const note_offset {
    {"cb", 11}, {"c",  0}, {"c#",  1},
    {"db",  1}, {"d",  2}, {"d#",  3},
    {"eb",  3}, {"e",  4}, {"e#",  5},
    {"fb",  5}, {"f",  5}, {"f#",  6},
    {"gb",  6}, {"g",  7}, {"g#",  8},
    {"ab",  8}, {"a",  9}, {"a#", 10},
    {"bb", 10}, {"b", 11}, {"b#",  0},
  };
  int const octave {std::stoi((*m)[2])};
  int const offset {note_offset.at((*m)[1])};
  int const semitones {(octave * 12) + offset - 57};
  return 0.01 * std::round((a4 * std::pow(std::pow(2.0, 1.0/12.0), semitones)) * 100.0);
}

Wave make_wave(Data const& data) {
  Wave wave {data.chan > 2 ? 2 : data.chan, data.rate, 0, std::vector<short>()};
  int const bits {16};
  bool const sign {true};
  double const max_amplitude {(std::pow(2, (sign ? bits - 1 : bits))) - 1};
  std::size_t const size {static_cast<std::size_t>((data.time < 1 ? 1 : data.time) * wave.sample_rate)};
  wave.num_samples += static_cast<int>(size) * wave.num_channels;
  wave.samples.reserve(static_cast<std::size_t>(wave.num_samples));

  if (data.wave == "sine") {
    for (std::size_t i = 0; i < size; ++i) {
      for (std::size_t j = 0; j < static_cast<std::size_t>(wave.num_channels); ++j) {
        if (((data.chan == Channel::Right) && (j == 0)) || ((data.chan == Channel::Left) && (j == 1))) {
          wave.samples.emplace_back(0);
        }
        else {
          auto const sample {data.ampl * max_amplitude * std::sin((2.0 * M_PI * (data.freq / wave.sample_rate * i)))};
          wave.samples.emplace_back(sample);
        }
      }
    }
  }
  else if (data.wave == "triangle") {
    for (std::size_t i = 0; i < size; ++i) {
      for (std::size_t j = 0; j < static_cast<std::size_t>(wave.num_channels); ++j) {
        if (((data.chan == Channel::Right) && (j == 0)) || ((data.chan == Channel::Left) && (j == 1))) {
          wave.samples.emplace_back(0);
        }
        else {
          wave.samples.emplace_back(((2 * (data.ampl * max_amplitude)) / M_PI) * std::asin(std::sin((2 * M_PI * (data.freq / wave.sample_rate)) * i)));
        }
      }
    }
  }
  else if (data.wave == "square") {
    for (std::size_t i = 0; i < size; ++i) {
      for (std::size_t j = 0; j < static_cast<std::size_t>(wave.num_channels); ++j) {
        if (((data.chan == Channel::Right) && (j == 0)) || ((data.chan == Channel::Left) && (j == 1))) {
          wave.samples.emplace_back(0);
        }
        else {
          auto sample {std::sin((2.0 * M_PI * (data.freq / wave.sample_rate * i)))};
          if (sample >= 0) {
            sample = data.ampl * (max_amplitude - 1);
          }
          else if (sample < 0) {
            sample = data.ampl * -max_amplitude;
          }
          wave.samples.emplace_back(sample);
        }
      }
    }
  }
  else if (data.wave == "saw") {
    for (std::size_t i = 0; i < size; ++i) {
      for (std::size_t j = 0; j < static_cast<std::size_t>(wave.num_channels); ++j) {
        // TODO is the impl for saw wave generation correct?
        // f(x) = -1 * ((2 * a) / pi) * atan(cot(((x * pi) / p)))
        if (((data.chan == Channel::Right) && (j == 0)) || ((data.chan == Channel::Left) && (j == 1))) {
          wave.samples.emplace_back(0);
        }
        else {
          wave.samples.emplace_back(-1 * ((2 * (data.ampl * max_amplitude)) / M_PI) * std::atan(std::tan(M_PI_2 - (((i * M_PI) / (data.freq / wave.sample_rate))))));
        }
      }
    }
  }

  return wave;
}

Track make_track(Wave const& wave, bool const loop) {
  Track track;
  if (!track.buf.loadFromSamples(wave.samples.data(), static_cast<sf::Uint64>(wave.num_samples), static_cast<unsigned int>(wave.num_channels), static_cast<unsigned int>(wave.sample_rate))) {
    throw std::runtime_error("failed to load audio from sample");
  }
  track.sound.setLoop(loop);
  track.sound.setPitch(1);
  track.sound.setVolume(100);
  track.sound.setPosition(0, 0, 0);
  track.sound.setRelativeToListener(true);
  track.sound.setBuffer(track.buf);
  return track;
}

bool is_playing(Track const& track) {
  return track.sound.getStatus() == sf::Sound::Playing;
}

void draw_wave(Wave const& wave, Data const& data, Track const* track) {
  std::size_t width {0};
  std::size_t height {0};
  OB::Term::size(width, height);
  if (width > 20) {
    width -= 20;
    // TODO why isnt height under 10 evenly spacing notes
    height = height > 10 ? 10 : height - 2;

    std::cout << aec::cursor_hide << aec::cursor_up(1) << std::flush;

    std::size_t curs_x {0};
    std::size_t curs_y {0};
    aec::cursor_get(curs_x, curs_y);
    cursor_y = curs_y;

    std::size_t const wave_size {wave.samples.size() / static_cast<std::size_t>(wave.num_channels)};
    std::size_t const wave_period {static_cast<std::size_t>(std::round(wave.sample_rate / data.freq))};
    // TODO if screen size is smaller than wave period, scroll animate the wave
    for (std::size_t x = 0; x < width && x < wave_size && x < wave_period; ++x) {
      double s {0};
      if (data.chan == Channel::Right) {
        s = wave.samples[(1 + x * static_cast<std::size_t>(wave.num_channels)) + (data.chan == Channel::Right ? 1 : 0)];
      }
      else {
        s = wave.samples[(x * static_cast<std::size_t>(wave.num_channels)) + (data.chan == Channel::Right ? 1 : 0)];
      }
      std::size_t const y {static_cast<std::size_t>(std::round(scale(s, -32768.0, 32767.0, 0.0, static_cast<double>(height))))};
      if (use_color) {
        OB::Prism::HSLA color {0, 100, 50, 1.0};
        color.h(color.h() - ((360.0 / height) * y));
        std::cout << aec::cursor_set(x + 21, curs_y - y) << aec::clear << term_fg(OB::Prism::RGBA(color)) << data.graphic;
      }
      else {
        std::cout << aec::cursor_set(x + 21, curs_y - y) << data.graphic;
      }
      if (track) {
        std::cout << std::flush;
        if (is_playing(*track)) {
          sleep(std::chrono::milliseconds(static_cast<int>(std::round(data.time * 1000 / (wave_period < width ? wave_period : width)))));
        }
      }
    }

    std::cout << aec::clear << aec::cursor_set(1, curs_y) << "\n" << aec::cursor_show << std::flush;
  }
}

void save_to_file(Wave const& wave, std::string const& output) {
  sf::SoundBuffer buf;
  if (!buf.loadFromSamples(wave.samples.data(), static_cast<sf::Uint64>(wave.num_samples), static_cast<unsigned int>(wave.num_channels), static_cast<unsigned int>(wave.sample_rate))) {
    throw std::runtime_error("failed to load audio from sample");
  }
  if (!buf.saveToFile(output)) {
    throw std::runtime_error("failed to save audio to '" + output + "'");
  }
}

Data make_data(Parg& pg) {
  // TODO validate all user passed args
  Data data;

  data.graphic = pg.get<std::string>("char");

  data.a4 = pg.get<double>("a4");
  data.sos = pg.get<double>("sos");

  {
    data.freq = data.a4;
    auto freq_str {pg.get_pos()};
    if (freq_str.size()) {
      try {
        data.freq = std::stod(freq_str);
      }
      catch (...) {
        data.freq = note_to_freq(freq_str);
      }
    }
  }

  {
    data.chan = Channel::Mono;
    auto chan_str = pg.get<std::string>("channels");
    if (chan_str == "2" || chan_str == "stereo") {
      data.chan = Channel::Stereo;
    }
    else if (chan_str == "left") {
      data.chan = Channel::Left;
    }
    else if (chan_str == "right") {
      data.chan = Channel::Right;
    }
  }

  data.time = pg.get<double>("time");
  data.loop = pg.get<bool>("loop");
  if (data.loop && data.time == 0) {data.time = 1;}

  data.wave = pg.get<std::string>("wave");
  data.rate = pg.get<int>("rate");
  data.ampl = pg.get<double>("amplitude");
  data.size = data.sos / data.freq;

  return data;
}

void print_data(Data const& data) {
  struct Style {
    std::string punc {aec::fg_true("c0c0c0")};
    std::string key {aec::fg_true("ff54ff")};
    std::string value {aec::fg_true("54ff54")};
    std::string unit {aec::fg_true("c0c0c0")};
  };
  Style style;

  auto const print_kv = [&](auto const& key, auto const& value) {
    std::cout << aec::wrap(key, style.key, use_color) << aec::wrap(": ", style.punc, use_color) << aec::wrap(value, style.value, use_color) << "\n";
  };

  auto const print_kvu = [&](auto const& key, auto const& value, auto const& unit) {
    std::cout << aec::wrap(key, style.key, use_color) << aec::wrap(": ", style.punc, use_color) << aec::wrap(value, style.value, use_color) << " " << aec::wrap(unit, style.unit, use_color) << "\n";
  };

  print_kvu("  a4", data.a4, "Hz");
  print_kvu(" sos", data.sos, "m/s");
  print_kv("note", freq_to_note(data.freq, data.a4));
  print_kvu("freq", data.freq, "Hz");
  print_kvu("size", data.size, "m");
  print_kv("wave", data.wave);
  print_kvu("rate", data.rate, "Hz");
  print_kv("ampl", data.ampl);
  print_kv("chan", channel_str.at(static_cast<std::size_t>(data.chan)));
  print_kvu("time", data.time, "s");
  print_kv("loop", data.loop);
}

int main(int argc, char** argv) {
  std::ios_base::sync_with_stdio(false);

  Parg pg {argc, argv};
  auto const pg_status {program_info(pg)};
  if (pg_status > 0) return 0;
  if (pg_status < 0) return 1;

  is_term = OB::Term::is_term(STDOUT_FILENO);
  use_color = pg.get<std::string>("colour") == "auto" ?
    is_term : pg.get<std::string>("colour") == "on";

  try {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto data = make_data(pg);
    print_data(data);

    auto wave = make_wave(data);
    if (pg.find("output")) {
      save_to_file(wave, pg.get<std::string>("output"));
    }
    else if (data.time > 0.0) {
      auto track = make_track(wave, data.loop);
      track.sound.play();
      if (is_term) {draw_wave(wave, data, &track);}
      while (is_playing(track)) {sleep(std::chrono::milliseconds(20));}
    }
    else {
      if (is_term) {draw_wave(wave, data);}
    }
  }
  catch(std::exception const& e) {
    std::cerr
    << "\n"
    << aec::wrap("Error: ", pg.style.error, use_color)
    << e.what()
    << "\n";

    return 1;
  }
  catch(...) {
    std::cerr
    << "\n"
    << aec::wrap("Error: ", pg.style.error, use_color)
    << "an unexpected error occurred"
    << "\n";

    return 1;
  }

  return 0;
}
