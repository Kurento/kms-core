/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

namespace kurento
{

class RandomGenerator
{
  boost::uuids::basic_random_generator<boost::mt19937> gen;
  boost::mt19937 ran;
  pid_t pid;

public:
  RandomGenerator () : gen (&ran)
  {
    init ();
  }

  void init ()
  {
    std::chrono::high_resolution_clock::time_point now =
      std::chrono::high_resolution_clock::now();

    std::chrono::nanoseconds time =
      std::chrono::duration_cast<std::chrono::nanoseconds>
      (now.time_since_epoch () );

    ran.seed (time.count() );

    pid = getpid();
  }

  void reinit ()
  {
    if (pid != getpid() ) {
      init();
    }
  }

  std::string getUUID ()
  {
    reinit();

    std::stringstream ss;
    boost::uuids::uuid uuid = gen ();

    ss << uuid;
    return ss.str();
  }
};

static RandomGenerator gen;

std::string
generateUUID ()
{
  return gen.getUUID ();
}

}
