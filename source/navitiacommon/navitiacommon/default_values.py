#  Copyright (c) 2001-2014, Canal TP and/or its affiliates. All rights reserved.
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

"""
This file contains the default values for parameters used in the journey planner.
This parameters are used on the creation of the instances in the tyr database, they will not be updated automatically.
This parameters can be used directly by jormungandr if the instance is not known in tyr, typically in development setup
"""

import logging
import sys

max_walking_duration_to_pt = 15*60

max_bike_duration_to_pt = 15*60

max_bss_duration_to_pt = 15*60

max_car_duration_to_pt = 30*60

walking_speed = 1.12

bike_speed = 4.1

bss_speed = 4.1

car_speed = 11.11

max_nb_transfers = 10

journey_order = 'arrival_time'

destineo_min_tc_with_car = 5*60

destineo_min_tc_with_bike = 5*60

destineo_min_tc_with_bss = 5*60

destineo_min_bike = 4*60

destineo_min_bss = 4*60 + 3*60 #we want 4minute on the bike, so we add the time to pick and put back the bss

destineo_min_car = 5*60

#if a journey is X time longer than the earliest one we remove it
factor_too_long_journey = 4

#all journeys with a duration fewer than this value will be kept no matter what even if they are 20 times slower than the earliest one
min_duration_too_long_journey = 15*60


def get_value_or_default(attr, instance, instance_name):
    if not instance or getattr(instance, attr) == None:
        logger = logging.getLogger(__name__)
        value = getattr(sys.modules[__name__], attr)
        logger.warn('instance %s not found in db, we use the default value (%s) for the param %s', instance_name, value, attr)
        return value
    else:
        return getattr(instance, attr)
