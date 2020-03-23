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

#ifndef INFO_HH
#define INFO_HH

#include "ob/parg.hh"
#include "ob/term.hh"

#include <cstddef>

#include <string>
#include <string_view>
#include <iostream>

inline int program_info(OB::Parg& pg);
inline bool program_color(std::string_view color);
inline void program_init(OB::Parg& pg);

inline void program_init(OB::Parg& pg)
{
  pg.name("gentone").version("0.1.1 (23.03.2020)");
  pg.description("Generate a tone from a note or frequency.");

  pg.usage("[Hz|A-G[b#]0-8] [--colour=<on|off|auto>] [-l|--loop] [--char=<char>] [--a4=<Hz>] [--speed=<m/s>] [-w|--wave=<sine|square|triangle|saw>] [-t|--time=<seconds>] [-c|--channels=<1|2|mono|stereo|left|right>] [-r|--rate=<Hz>] [-a|--amplitude=<0.0-1.0>] [-o|--output=<file>]");
  pg.usage("[--colour=<on|off|auto>] -h|--help");
  pg.usage("[--colour=<on|off|auto>] -v|--version");
  pg.usage("[--colour=<on|off|auto>] --license");

  pg.info({"Examples", {
    {"gentone",
      "Run the program."},
    {"gentone --time 3 A4",
      "Generate a 3 second mono sine wave using the musical note A4."},
    {"gentone --time 3 --channels 2 --wave triangle 440",
      "Generate a 3 second stereo triangle wave with a frequency of 440Hz."},
    {"gentone --time 1 --output sine.wav C#7",
      "Generate a 1 second mono sine wave using the musical note C#7 and save the tone to the output file 'sine.wav'."},
    {"gentone --help --colour=off",
      "Print the help output, without colour."},
    {"gentone --help",
      "Print the help output."},
    {"gentone --version",
      "Print the program version."},
    {"gentone --license",
      "Print the program license."},
  }});

  pg.info({"Exit Codes", {
    {"0", "normal"},
    {"1", "error"},
  }});

  pg.info({"Meta", {
    {"", "The version format is 'major.minor.patch (day.month.year)'."},
  }});

  pg.info({"Repository", {
    {"", "https://github.com/octobanana/gentone.git"},
  }});

  pg.info({"Homepage", {
    {"", "https://octobanana.com/software/gentone"},
  }});

  pg.author("Brett Robinson (octobanana) <octobanana.dev@gmail.com>");

  // general flags
  pg.set("help,h", "Print the help output.");
  pg.set("version,v", "Print the program version.");
  pg.set("license", "Print the program license.");

  // options
  pg.set("colour", "auto", "on|off|auto", "Print the program output with colour either on, off, or auto based on if stdout is a tty, the default value is 'auto'.");

  pg.set("loop,l", "Loop the generated tone.");
  pg.set("char", "*", "char", "The character used to draw the wave diagram.");
  pg.set("a4", "440", "Hz", "The standard pitch frequency used for the A above middle C.");
  pg.set("sos", "343", "m/s", "The speed of sound.");
  pg.set("wave,w", "sine", "sine|square|triangle|saw", "The type of waveform used to generate the tone.");
  pg.set("time,t", "0", "seconds", "The duration of the tone in seconds.");
  pg.set("channels,c", "1", "1|2|mono|stereo|left|right", "The number of channels to use, 1 is mono, 2 is stereo.");
  pg.set("rate,r", "44100", "Hz", "The sample rate used to generate the tone.");
  pg.set("amplitude,a", "1", "0.0-1.0", "The max amplitude of the generated tone.");
  pg.set("output,o", "", "file", "Save the generated tone to a file.");

  // allow and capture positional arguments
  pg.set_pos();
}

inline bool program_color(std::string_view color)
{
  if (color == "on")
  {
    // color on
    return true;
  }

  if (color == "off")
  {
    // color off
    return false;
  }

  // color auto
  return OB::Term::is_term(STDOUT_FILENO);
}

inline int program_info(OB::Parg& pg)
{
  // init info/options
  program_init(pg);

  // parse options
  auto const status {pg.parse()};

  // set output color choice
  pg.color(program_color(pg.get<std::string>("colour")));

  if (status < 0)
  {
    // an error occurred
    std::cerr
    << pg.usage()
    << "\n"
    << pg.error();

    return -1;
  }

  if (pg.get<bool>("help"))
  {
    // show help output
    std::cout << pg.help();

    return 1;
  }

  if (pg.get<bool>("version"))
  {
    // show version output
    std::cout << pg.version();

    return 1;
  }

  if (pg.get<bool>("license"))
  {
    // show license output
    std::cout << pg.license();

    return 1;
  }

  // success
  return 0;
}

#endif // INFO_HH
