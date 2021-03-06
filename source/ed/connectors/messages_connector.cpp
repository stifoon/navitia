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

#include "messages_connector.h"
#include <pqxx/pqxx>
#include <boost/make_shared.hpp>
#include "utils/exception.h"
#include "utils/logger.h"
#include "type/data.h"
#include "type/pt_data.h"
#include <boost/dynamic_bitset.hpp>
#include <boost/date_time/local_time/local_time.hpp>

namespace ed{ namespace connectors{

namespace pt = boost::posix_time;
using nt::new_disruption::Severity;
using nt::new_disruption::Impact;
using nt::new_disruption::Effect;
using nt::new_disruption::DisruptionHolder;

static boost::shared_ptr<Severity>
get_or_create_severity(DisruptionHolder& disruptions, int id) {
    std::string name;
    if (id == 0) {
        name = "information";
    } else if (id == 1) {
        name = "warning";
    } else if (id == 2) {
        name = "disrupt";
    } else {
        throw navitia::exception("Disruption: invalid severity in database");
    }

    auto it = disruptions.severities.find(name);

    if (it != disruptions.severities.end()) {
        // we return the weak_ptr if it is still alive, else we'll add a new one
        if (auto res = it->second.lock()) return res;
    }

    auto severity = boost::make_shared<Severity>();
    disruptions.severities.insert({name, severity});

    severity->uri = name;
    severity->effect = Effect::UNKNOWN_EFFECT;

    return severity;
}

void load_disruptions(
        navitia::type::PT_Data& pt_data,
        const RealtimeLoaderConfig& conf,
        const boost::posix_time::ptime& current_time){
    using boost::local_time::local_date_time;
    //pour le moment on vire les timezone et on considére que c'est de l'heure local
    std::string request = "SELECT id, uri, start_publication_date::timestamp, "
        "end_publication_date::timestamp, start_application_date::timestamp, "
        "end_application_date::timestamp, start_application_daily_hour::time, "
        "end_application_daily_hour::time, active_days, object_uri, "
        "object_type_id, language, body, title, message_status_id "
        "FROM realtime.message m "
        "JOIN realtime.localized_message l ON l.message_id = m.id "
        "where end_publication_date >= ($1::timestamp - $2::interval) "
        "order by id";
    //on tris par id pour les regrouper ensemble
    pqxx::result result;
    std::unique_ptr<pqxx::connection> conn;
    try{
        conn = std::unique_ptr<pqxx::connection>(new pqxx::connection(conf.connection_string));

#if PQXX_VERSION_MAJOR < 4
        conn->prepare("messages", request)("timestamp")("INTERVAL", pqxx::prepare::treat_string);
#else
        conn->prepare("messages", request);
#endif

        std::string st_current_time = boost::posix_time::to_iso_string(current_time);
        std::string st_shift_days =  std::to_string(conf.shift_days) + " days";
        pqxx::work work(*conn, "chargement des messages");
        result = work.prepared("messages")(st_current_time)(st_shift_days).exec();
    } catch(const pqxx::pqxx_exception& e) {
        throw navitia::exception(e.base().what());
    }

    nt::new_disruption::DisruptionHolder& disruptions = pt_data.disruption_holder;
    boost::shared_ptr<nt::new_disruption::Impact> impact;
    std::string current_uri = "";
    boost::local_time::time_zone_ptr zone(new boost::local_time::posix_time_zone("CET+1CEST,M3.5.0/2,M10.5.0/3"));
    for (auto cursor = result.begin(); cursor != result.end(); ++cursor) {
        //we can have several message for each impact, (and one impact by disruption)
        if (cursor["uri"].as<std::string>() != current_uri) {
            disruptions.disruptions.push_back(std::make_unique<nt::new_disruption::Disruption>());

            auto& disruption = disruptions.disruptions.back();
            cursor["uri"].to(current_uri);
            disruption->uri = current_uri;

            impact = boost::make_shared<nt::new_disruption::Impact>();
            cursor["uri"].to(impact->uri);
            auto start_pub = pt::time_from_string(cursor["start_publication_date"].as<std::string>());
            auto end_pub = pt::time_from_string(cursor["end_publication_date"].as<std::string>());

            disruption->publication_period = boost::posix_time::time_period(
                    local_date_time(start_pub.date(), start_pub.time_of_day(), zone,
                                    local_date_time::NOT_DATE_TIME_ON_ERROR).utc_time(),
                    local_date_time(end_pub.date(), end_pub.time_of_day(), zone,
                                    local_date_time::NOT_DATE_TIME_ON_ERROR).utc_time()
                    );

            //we need to handle the active days to split the period
            auto start_app = pt::time_from_string(cursor["start_application_date"].as<std::string>());
            auto end_app = pt::time_from_string(cursor["end_application_date"].as<std::string>());
            pt::ptime start = local_date_time(start_app.date(), start_app.time_of_day(),
                                              zone, local_date_time::NOT_DATE_TIME_ON_ERROR).utc_time();
            pt::ptime end = local_date_time(end_app.date(), end_app.time_of_day(),
                                              zone, local_date_time::NOT_DATE_TIME_ON_ERROR).utc_time();;

            auto daily_start_hour = pt::duration_from_string(
                        cursor["start_application_daily_hour"].as<std::string>());
            auto daily_end_hour = pt::duration_from_string(
                        cursor["end_application_daily_hour"].as<std::string>());

            std::bitset<7> active_days (cursor["active_days"].as<std::string>());

            impact->application_periods = navitia::expand_calendar(start, end,
                                                       daily_start_hour, daily_end_hour,
                                                       active_days);
            disruption->add_impact(impact);
        }
        auto message = nt::new_disruption::Message();
        cursor["body"].to(message.text);
        impact->messages.push_back(message);

        auto pt_object = nt::new_disruption::make_pt_obj(
            static_cast<navitia::type::Type_e>(cursor["object_type_id"].as<int>()),
            cursor["object_uri"].as<std::string>(),
            pt_data,
            impact
            );
        impact->informed_entities.push_back(pt_object);

        auto severity_id = cursor["message_status_id"].as<int>();
        auto severity = get_or_create_severity(disruptions, severity_id);
        impact->severity = severity;

        // message does not have tags, notes, localization or cause
    }
}

template <typename Container>
void add_impact(Container& map, const std::string& uri, const boost::shared_ptr<Impact>& impact) {
    auto it = map.find(uri);
    if(it != map.end()){
        it->second->add_impact(impact);
    }
}

std::vector<ed::AtPerturbation> load_at_perturbations(
        const RealtimeLoaderConfig& conf,
        const boost::posix_time::ptime& current_time){
    //pour le moment on vire les timezone et on considére que c'est de l'heure local
    std::string request = "SELECT id, uri, start_application_date::timestamp, "
        "end_application_date::timestamp, start_application_daily_hour::time, "
        "end_application_daily_hour::time, active_days, object_uri, "
        "object_type_id "
        "FROM realtime.at_perturbation "
        "where end_application_date >= ($1::timestamp - $2::interval) ";
    pqxx::result result;
    std::unique_ptr<pqxx::connection> conn;
    try{
        conn = std::unique_ptr<pqxx::connection>(new pqxx::connection(conf.connection_string));

#if PQXX_VERSION_MAJOR < 4
        conn->prepare("messages", request)("timestamp")("INTERVAL", pqxx::prepare::treat_string);
#else
        conn->prepare("messages", request);
#endif

        std::string st_current_time = boost::posix_time::to_iso_string(current_time);
        std::string st_shift_days =  std::to_string(conf.shift_days) + " days";
        pqxx::work work(*conn, "chargement des perturbations at");
        result = work.prepared("messages")(st_current_time)(st_shift_days).exec();
    } catch(const pqxx::pqxx_exception& e) {
        throw navitia::exception(e.base().what());
    }

    std::vector<ed::AtPerturbation> perturbations;
    for(auto cursor = result.begin(); cursor != result.end(); ++cursor){
        ed::AtPerturbation perturbation;
        cursor["uri"].to(perturbation.uri);
        //on construit le message
        cursor["object_uri"].to(perturbation.object_uri);

        perturbation.object_type = static_cast<navitia::type::Type_e>(
                cursor["object_type_id"].as<int>());

        perturbation.application_daily_start_hour = pt::duration_from_string(
                cursor["start_application_daily_hour"].as<std::string>());

        perturbation.application_daily_end_hour = pt::duration_from_string(
                cursor["end_application_daily_hour"].as<std::string>());

        pt::ptime start = pt::time_from_string(
                cursor["start_application_date"].as<std::string>());

        pt::ptime end = pt::time_from_string(
                cursor["end_application_date"].as<std::string>());
        perturbation.application_period = pt::time_period(start, end);

        perturbation.active_days = std::bitset<8>(
                cursor["active_days"].as<std::string>());

        perturbations.push_back(perturbation);
    }

    return perturbations;
}

}}//namespace
