/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.
  
This file is part of Navitia,
    the software to build cool stuff with public transport.
 
Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!
  
LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.
   
You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
  
Stay tuned using
twitter @navitia 
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#include "raptor_solutions.h"
#include "raptor_path.h"
#include "raptor.h"
#include "raptor_path_defs.h"

namespace bt = boost::posix_time;
namespace navitia { namespace routing {

Solutions
get_solutions(const std::vector<std::pair<SpIdx, navitia::time_duration> > &departs,
             const std::vector<std::pair<SpIdx, navitia::time_duration> > &destinations,
             bool clockwise, const type::AccessibiliteParams & accessibilite_params,
             bool disruption_active, const RAPTOR& raptor) {
      Solutions result;
      auto pareto_front = get_pareto_front(clockwise, departs, destinations,
              accessibilite_params, disruption_active, raptor);
      result.insert(pareto_front.begin(), pareto_front.end());

      if(!pareto_front.empty()) {
          auto walking_solutions = get_walking_solutions(clockwise, departs, destinations,
                  *pareto_front.rbegin(), disruption_active, accessibilite_params, raptor);
          if(!walking_solutions.empty()) {
            result.insert(walking_solutions.begin(), walking_solutions.end());
          }
      }
      return result;
}


Solutions
get_solutions(const std::vector<std::pair<SpIdx, navitia::time_duration> > &departs,
              const DateTime &dep, bool clockwise, const RAPTOR& raptor, bool) {
    Solutions result;
    for(auto dep_dist : departs) {
        for(const auto* jpp: raptor.get_sp(dep_dist.first)->journey_pattern_point_list) {
            Solution d;
            d.count = 0;
            d.jpp_idx = JppIdx(*jpp);
            d.walking_time = dep_dist.second;
            if(clockwise)
                d.arrival = dep + d.walking_time.total_seconds();
            else
                d.arrival = dep - d.walking_time.total_seconds();
            result.insert(d);
        }
    }
    return result;
}

static DateTime
combine_dt_walking_time(bool clockwise, const DateTime& current, int walking_duration) {
    return clockwise ? current - walking_duration : current + walking_duration;
}

// Does the current date improves compared to best_so_far – we must not forget to take the walking duration
static bool improves(const DateTime& best_so_far,
                     bool clockwise,
                     const DateTime& current,
                     int walking_duration) {
    const auto dt = combine_dt_walking_time(clockwise, current, walking_duration);
    return clockwise ? dt > best_so_far : dt < best_so_far;
}

static bool is_equal(const DateTime& best_so_far,
                     bool clockwise,
                     const DateTime& current,
                     int walking_duration) {
    const auto dt = combine_dt_walking_time(clockwise, current, walking_duration);
    return dt == best_so_far;
}

static size_t nb_jpp_of_path(int count,
                             JppIdx jpp_idx,
                             bool clockwise,
                             bool disruption_active,
                             const type::AccessibiliteParams& accessibilite_params,
                             const RAPTOR& raptor) {
    struct VisitorNbJPP : public BasePathVisitor {
        size_t nb_jpp = 0;
        void loop_vj(const type::StopTime*, boost::posix_time::ptime, boost::posix_time::ptime) {
            ++nb_jpp;
        }

        void change_vj(const type::StopTime*, const type::StopTime*,
                       boost::posix_time::ptime ,boost::posix_time::ptime,
                       bool) {
            ++nb_jpp;
        }
    };
    VisitorNbJPP v;
    read_path(v, jpp_idx, count, !clockwise, disruption_active, accessibilite_params, raptor);
    return v.nb_jpp;
}

Solutions
get_pareto_front(bool clockwise, const std::vector<std::pair<SpIdx, navitia::time_duration> > &departs,
               const std::vector<std::pair<SpIdx, navitia::time_duration> > &destinations,
               const type::AccessibiliteParams & accessibilite_params, bool disruption_active, const RAPTOR& raptor) {
    Solutions result;

    DateTime best_dt, best_dt_jpp;
    if(clockwise) {
        best_dt = DateTimeUtils::min;
        best_dt_jpp = DateTimeUtils::min;
    } else {
        best_dt = DateTimeUtils::inf;
        best_dt_jpp = DateTimeUtils::inf;
    }
    for(unsigned int round=1; round <= raptor.count; ++round) {
        // For every round with look for the best journey pattern point that belongs to one of the destination stop points
        // We must not forget to walking duration
        JppIdx best_jpp = JppIdx();
        size_t best_nb_jpp_of_path = std::numeric_limits<size_t>::max();
        for(auto spid_dist : destinations) {
            for(auto journey_pattern_point : raptor.get_sp(spid_dist.first)->journey_pattern_point_list) {
                const JppIdx jppidx = JppIdx(*journey_pattern_point);
                const auto& ls = raptor.labels[round];
                if (!ls.pt_is_initialized(jppidx)) {
                    continue;
                }
                size_t nb_jpp = nb_jpp_of_path(round, jppidx, clockwise, disruption_active,
                                               accessibilite_params, raptor);
                const auto& dt_pt = ls.dt_pt(jppidx);
                if(!improves(best_dt, clockwise, dt_pt, spid_dist.second.total_seconds())) {
                    if (!is_equal(best_dt, clockwise, dt_pt, spid_dist.second.total_seconds()) ||
                             best_nb_jpp_of_path <= nb_jpp) {
                        continue;
                    }
                }
                best_jpp = jppidx;
                best_dt_jpp = dt_pt;
                best_nb_jpp_of_path = nb_jpp;
                // When computing with clockwise, in the second pass we store deparutre time
                // in labels, but we want arrival time, so we need to retrive the good stop_time
                // with best_stop_time
                const type::StopTime* st = nullptr;
                DateTime dt = 0;

                std::tie(st, dt) = get_current_stidx_gap(round,
                                                         jppidx,
                                                         raptor.labels,
                                                         accessibilite_params,
                                                         !clockwise,
                                                         raptor,
                                                         disruption_active);
                if(st != nullptr) {
                    if(clockwise) {
                        auto arrival_time = !st->is_frequency() ? 
                                st->arrival_time :
                                st->f_arrival_time(DateTimeUtils::hour(dt));
                        DateTimeUtils::update(best_dt_jpp, arrival_time, false);
                    } else {
                        auto departure_time = !st->is_frequency() ?
                            st->departure_time :
                            st->f_departure_time(DateTimeUtils::hour(dt));
                        DateTimeUtils::update(best_dt_jpp, departure_time, true);
                    }
                    if(clockwise)
                        best_dt = dt_pt - spid_dist.second.total_seconds();
                    else
                        best_dt = dt_pt + spid_dist.second.total_seconds();
                }
            }
        }
        if (best_jpp.is_valid()) {
            Solution s;
            s.jpp_idx = best_jpp;
            s.count = round;
            s.walking_time = getWalkingTime(round, best_jpp, departs, destinations, clockwise, disruption_active,
                                            accessibilite_params, raptor);
            s.arrival = best_dt_jpp;
            s.ratio = 0;
            s.total_arrival = best_dt;
            JppIdx final_jpp_idx;
            std::tie(final_jpp_idx, s.upper_bound) = get_final_jppidx_and_date(round, best_jpp, clockwise,
                                                                             disruption_active, accessibilite_params, raptor);
            for(auto spid_dep : departs) {
                if (SpIdx(*raptor.get_jpp(final_jpp_idx)->stop_point) == spid_dep.first) {
                    if(clockwise) {
                        s.upper_bound = s.upper_bound + spid_dep.second.total_seconds();
                    }else {
                        s.upper_bound = s.upper_bound - spid_dep.second.total_seconds();
                    }
                }
            }
            result.insert(s);
        }
    }

    return result;
}





Solutions
get_walking_solutions(bool clockwise, const std::vector<std::pair<SpIdx, navitia::time_duration> > &departs,
                      const std::vector<std::pair<SpIdx, navitia::time_duration> > &destinations, const Solution& best,
                      const bool disruption_active, const type::AccessibiliteParams &accessibilite_params,
                      const RAPTOR& raptor) {
    Solutions result;

    std::map<JppIdx, Solution> tmp;
    // We start at 1 because we don't want results of the first round
    for(uint32_t i=1; i <= raptor.count; ++i) {
        for(auto spid_dist : destinations) {
            Solution best_departure;
            best_departure.ratio = 2;
            best_departure.jpp_idx = JppIdx();
            for(auto journey_pattern_point: raptor.get_sp(spid_dist.first)->journey_pattern_point_list) {
                JppIdx jppidx = JppIdx(*journey_pattern_point);
                // We only want solution ending by a vehicle journey or a stay_in
                if(raptor.labels[i].pt_is_initialized(jppidx)) {
                    navitia::time_duration walking_time = getWalkingTime(i, jppidx, departs, destinations, clockwise,
                                                                         disruption_active, accessibilite_params, raptor);
                    if(best.walking_time < walking_time) {
                        continue;
                    }
                    float lost_time;
                    if(clockwise)
                        lost_time = best.total_arrival -
                            (raptor.labels[i].dt_pt(jppidx) - best.walking_time.total_seconds());
                    else
                        lost_time = (raptor.labels[i].dt_pt(jppidx) + spid_dist.second.total_seconds()) -
                            best.total_arrival;


                    //Si je gagne 5 minutes de marche a pied, je suis pret à perdre jusqu'à 10 minutes.
                    int walking_time_diff_in_s = (best.walking_time - walking_time).total_seconds();
                    if (walking_time_diff_in_s > 0) {
                        float ratio = lost_time / walking_time_diff_in_s;
                        if( ratio >= best_departure.ratio) {
                            continue;
                        }
                        Solution s;
                        s.jpp_idx = jppidx;
                        s.count = i;
                        s.ratio = ratio;
                        s.walking_time = walking_time;
                        s.arrival = raptor.labels[i].dt_pt(jppidx);
                        JppIdx final_jpp_idx;
                        DateTime last_time;
                        std::tie(final_jpp_idx, last_time) = get_final_jppidx_and_date(i, jppidx, clockwise,
                                            disruption_active, accessibilite_params, raptor);
                        const SpIdx final_sp_idx = SpIdx(*raptor.get_jpp(final_jpp_idx)->stop_point);

                        if(clockwise) {
                            s.upper_bound = last_time;
                            for(auto spid_dep : departs) {
                                if(final_sp_idx == spid_dep.first) {
                                    s.upper_bound = s.upper_bound + (spid_dep.second.total_seconds());
                                }
                            }
                        } else {
                            s.upper_bound = last_time;
                            for(auto spid_dep : departs) {
                                if(final_sp_idx == spid_dep.first) {
                                    s.upper_bound = s.upper_bound - (spid_dep.second.total_seconds());
                                }
                            }
                        }
                        best_departure = s;
                    }
                }
            }
            if (best_departure.jpp_idx.is_valid()) {
                if(tmp.find(best_departure.jpp_idx) == tmp.end()) {
                    tmp.insert(std::make_pair(best_departure.jpp_idx, best_departure));
                } else if(tmp[best_departure.jpp_idx].ratio > best_departure.ratio) {
                    tmp[best_departure.jpp_idx] = best_departure;
                }
            }
        }
    }


    std::vector<Solution> to_sort;
    for(auto p : tmp) {
        to_sort.push_back(p.second);
    }
    std::sort(to_sort.begin(), to_sort.end(),
            [](const Solution s1, const Solution s2) { return s1.ratio < s2.ratio;});

    if(to_sort.size() > 2)
        to_sort.resize(2);
    result.insert(to_sort.begin(), to_sort.end());

    return result;
}

struct VisitorFinalJppAndDate : public BasePathVisitor {
    JppIdx current_jpp = JppIdx();
    DateTime last_time = DateTimeUtils::inf;
    void final_step(const JppIdx current_jpp, size_t count, const std::vector<Labels> &labels) {
        this->current_jpp = current_jpp;
        last_time = labels[count].dt_pt(current_jpp);
    }
};

// Reparcours l’itinéraire rapidement pour avoir le JPP et la date de départ (si on cherchait l’arrivée au plus tôt)
std::pair<JppIdx, DateTime>
get_final_jppidx_and_date(int count, JppIdx jpp_idx, bool clockwise, bool disruption_active,
                          const type::AccessibiliteParams & accessibilite_params, const RAPTOR& raptor) {
    VisitorFinalJppAndDate v;
    read_path(v, jpp_idx, count, !clockwise, disruption_active, accessibilite_params, raptor);
    return std::make_pair(v.current_jpp, v.last_time);
}


struct VisitorWalkingTime : public BasePathVisitor {
    JppIdx departure_jpp_idx = JppIdx();

    void final_step(JppIdx current_jpp, size_t , const std::vector<Labels> &){
        departure_jpp_idx = current_jpp;
    }

};


navitia::time_duration getWalkingTime(int count, JppIdx jpp_idx, const std::vector<std::pair<SpIdx, navitia::time_duration> > &departs,
                     const std::vector<std::pair<SpIdx, navitia::time_duration> > &destinations,
                     bool clockwise, bool disruption_active, const type::AccessibiliteParams & accessibilite_params,
                     const RAPTOR& raptor) {

    const auto* current_jpp = raptor.get_jpp(jpp_idx);
    navitia::time_duration walking_time = {};

    //Marche à la fin
    for(auto dest_dist : destinations) {
        if(dest_dist.first == SpIdx(*current_jpp->stop_point)) {
            walking_time = dest_dist.second;
            break;
        }
    }

    VisitorWalkingTime v;
    read_path(v, jpp_idx, count, !clockwise, disruption_active, accessibilite_params, raptor);
    if (!v.departure_jpp_idx.is_valid()) {
        return walking_time;
    }
    const auto* departure_jpp = raptor.get_jpp(v.departure_jpp_idx);
    //Marche au départ
    for(auto dep_dist : departs) {
        if (dep_dist.first == SpIdx(*departure_jpp->stop_point)) {
            walking_time += dep_dist.second;
            break;
        }
    }

    return walking_time;
}

}}

