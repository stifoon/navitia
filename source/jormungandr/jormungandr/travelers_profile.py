# Copyright (c) 2001-2014, Canal TP and/or its affiliates. All rights reserved.
#
# This file is part of Navitia,
#     the software to build cool stuff with public transport.
#
# Hope you'll enjoy and contribute to this project,
#     powered by Canal TP (www.canaltp.fr).
# Help us simplify mobility and open public transport:
#     a non ending quest to the responsive locomotion way of traveling!
#
# LICENCE: This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Stay tuned using
# twitter @navitia
# IRC #navitia on freenode
# https://groups.google.com/d/forum/navitia
# www.navitia.io

from collections import OrderedDict

class TravelerProfile(object):
    def __init__(self, walking_speed=1.12, bike_speed=3.33, car_speed=11.11, max_duration_to_pt=15*60,
                 first_section_mode=[], last_section_mode=[], wheelchair=False, first_and_last_section_mode=[],
                 keolis_type_map={}):
        self.walking_speed = walking_speed
        self.bike_speed = bike_speed
        self.car_speed = car_speed

        self.max_duration_to_pt = max_duration_to_pt

        if first_and_last_section_mode:
            self.first_section_mode = self.last_section_mode = first_and_last_section_mode
        else:
            self.first_section_mode = first_section_mode
            self.last_section_mode = last_section_mode

        self.wheelchair = wheelchair

        self.keolis_type_map = keolis_type_map

    @property
    def bss_speed(self):
        return self.bike_speed

    def override_params(self, args):
        args['walking_speed'] = self.walking_speed
        args['bike_speed'] = self.bike_speed
        args['bss_speed'] = self.bss_speed
        args['car_speed'] = self.car_speed
        args['max_duration_to_pt'] = self.max_duration_to_pt

        args['origin_mode'] = self.first_section_mode
        args['destination_mode'] = self.last_section_mode

        args['wheelchair'] = self.wheelchair


travelers_profile = {
    'standard': TravelerProfile(walking_speed=1.39,
                                bike_speed=3.33,
                                max_duration_to_pt=12*60,
                                first_and_last_section_mode=['walking', 'bss'],
                                keolis_type_map={'rapid': ['best'],
                                    'comfort': ['less_fallback_walk', 'less_fallback_bss'],
                                    'healthy': ['non_pt_walk', 'non_pt_bss', 'comfort', 'fastest']}),

    'slow_walker': TravelerProfile(walking_speed=0.83,
                                   max_duration_to_pt=20*60,
                                   first_and_last_section_mode=['walking'],
                                   keolis_type_map={'rapid': ['best'],
                                       'comfort': ['less_fallback_walk'],
                                       'healthy': ['non_pt_walk', 'comfort', 'fastest']}),

    'fast_walker': TravelerProfile(walking_speed=1.67,
                                   bike_speed=3.33,
                                   max_duration_to_pt=20*60,
                                   first_and_last_section_mode=['walking', 'bss'],
                                   keolis_type_map={'rapid': ['best'],
                                       'comfort': ['less_fallback_walk', 'less_fallback_bss'],
                                       'healthy': ['non_pt_walk', 'non_pt_bss', 'comfort', 'fastest']}),

    'stroller': TravelerProfile(walking_speed=1.11,
                                max_duration_to_pt=15*60,
                                first_and_last_section_mode=['walking'],
                                wheelchair=True,
                                keolis_type_map={'rapid': ['best'],
                                    'comfort': ['less_fallback_walk'],
                                    'healthy': ['non_pt_walk', 'comfort', 'fastest']}),

    'wheelchair': TravelerProfile(walking_speed=0.83,
                                  max_duration_to_pt=20*60,
                                  first_and_last_section_mode=['walking'],
                                  wheelchair=True,
                                  keolis_type_map={'rapid': ['best'],
                                      'comfort': ['less_fallback_walk'],
                                      'healthy': ['non_pt_walk', 'comfort', 'fastest']}),

    'luggage': TravelerProfile(walking_speed=1.11,
                               max_duration_to_pt=15*60,
                               first_and_last_section_mode=['walking'],
                               wheelchair=True,
                               keolis_type_map={'rapid': ['best'],
                                   'comfort': ['less_fallback_walk'],
                                   'healthy': ['non_pt_walk', 'comfort', 'fastest']}),

    'heels': TravelerProfile(walking_speed=1.11,
                             bike_speed=3.33,
                             max_duration_to_pt=15*60,
                             first_and_last_section_mode=['walking', 'bss'],
                             keolis_type_map={'rapid': ['best'],
                                 'comfort': ['less_fallback_walk', 'less_fallback_bss'],
                                 'healthy': ['non_pt_walk', 'non_pt_bss', 'comfort', 'fastest']}),

    'scooter': TravelerProfile(walking_speed=2.22,
                               max_duration_to_pt=15*60,
                               first_and_last_section_mode=['walking'],
                               keolis_type_map={'rapid': ['best'],
                                   'comfort': ['less_fallback_walk'],
                                   'healthy': ['non_pt_walk', 'comfort', 'fastest']}),

    'cyclist': TravelerProfile(walking_speed=1.39,
                               bike_speed=3.33,
                               max_duration_to_pt=12*60,
                               first_section_mode=['walking', 'bike'],
                               last_section_mode=['walking'],
                               keolis_type_map={'rapid': ['best'],
                                   'comfort': ['less_fallback_walk', 'less_fallback_bss', 'less_fallback_bike'],
                                   'healthy': ['non_pt_walk', 'non_pt_bss', 'non_pt_bike', 'comfort', 'fastest']}),

    'motorist': TravelerProfile(walking_speed=1.11,
                                car_speed=11.11,
                                max_duration_to_pt=15*60,
                                first_section_mode=['walking', 'car'],
                                last_section_mode=['walking'],
                                keolis_type_map={'rapid': ['best'],
                                    'comfort': ['car', 'less_fallback_walk'],
                                    'healthy': ['non_pt_walk', 'comfort', 'fastest']}),
}
