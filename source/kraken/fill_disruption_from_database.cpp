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
#include <functional>

#include "fill_disruption_from_database.h"
#include <pqxx/pqxx>
#include "utils/exception.h"
#include "utils/logger.h"

#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>


namespace navitia {



void fill_disruption_from_database(const std::string& connection_string,
        navitia::type::PT_Data& pt_data, type::MetaData &meta, const std::vector<std::string>& contributors) {
    std::unique_ptr<pqxx::connection> conn;
    try{
        conn = std::unique_ptr<pqxx::connection>(new pqxx::connection(connection_string));
    }catch(const pqxx::pqxx_exception& e){
        throw navitia::exception(std::string("Unable to connect to chaos database: ")
                + std::string(e.base().what()));
    }
    pqxx::work work(*conn, "loading disruptions");

    size_t offset = 0,
           items_per_request = 100;
    DisruptionDatabaseReader reader(pt_data, meta);
    pqxx::result result;
    std::string contributors_array = boost::algorithm::join(contributors, ", ");
    do{
        std::string request = (boost::format(
               "SELECT "
               // Disruptions field
               "     d.id as disruption_id, d.reference as disruption_reference, d.note as disruption_note,"
               "     d.status as disruption_status,"
               "     extract(epoch from d.start_publication_date) :: bigint as disruption_start_publication_date,"
               "     extract(epoch from d.end_publication_date) :: bigint as disruption_end_publication_date,"
               "     extract(epoch from d.created_at) :: bigint as disruption_created_at,"
               "     extract(epoch from d.updated_at) :: bigint as disruption_updated_at,"
               "     co.contributor_code as contributor,"
               // Cause fields
               "     c.id as cause_id, c.wording as cause_wording,"
               "     c.is_visible as cause_visible,"
               "     extract(epoch from c.created_at) :: bigint as cause_created_at,"
               "     extract(epoch from c.updated_at) :: bigint as cause_updated_at,"
               // Tag fields
               "     t.id as tag_id, t.name as tag_name, t.is_visible as tag_is_visible,"
               "     extract(epoch from t.created_at) :: bigint as tag_created_at,"
               "     extract(epoch from t.updated_at) :: bigint as tag_updated_at,"
               // Impact fields
               "     i.id as impact_id, i.status as impact_status, i.disruption_id as impact_disruption_id,"
               "     extract(epoch from i.created_at) :: bigint as impact_created_at,"
               "     extract(epoch from i.updated_at) :: bigint as impact_updated_at,"
               // Application period fields
               "     a.id as application_id,"
               "     extract(epoch from a.start_date) :: bigint as application_start_date,"
               "     extract(epoch from a.end_date) :: bigint as application_end_date,"
               // Severity fields
               "     s.id as severity_id, s.wording as severity_wording, s.color as severity_color,"
               "     s.is_visible as severity_is_visible, s.priority as severity_priority,"
               "     s.effect as severity_effect,"
               "     extract(epoch from s.created_at) :: bigint as severity_created_at,"
               "     extract(epoch from s.updated_at) :: bigint as severity_updated_at,"
               // Ptobject fields
               "     p.id as ptobject_id, p.type as ptobject_type, p.uri as ptobject_uri,"
               "     extract(epoch from p.created_at) :: bigint as ptobject_created_at,"
               "     extract(epoch from p.updated_at) :: bigint as ptobject_updated_at,"
               // Ptobject line_section optional fields
               "     ls_line.uri as ls_line_uri,"
               "     extract(epoch from ls_line.created_at) :: bigint as ls_line_created_at,"
               "     extract(epoch from ls_line.updated_at) :: bigint as ls_line_updated_at,"
               "     ls_start.uri as ls_start_uri,"
               "     extract(epoch from ls_start.created_at) :: bigint as ls_start_created_at,"
               "     extract(epoch from ls_start.updated_at) :: bigint as ls_start_updated_at,"
               "     ls_end.uri as ls_end_uri,"
               "     extract(epoch from ls_end.created_at) :: bigint as ls_end_created_at,"
               "     extract(epoch from ls_end.updated_at) :: bigint as ls_end_updated_at,"
               // Message fields
               "     m.id as message_id, m.text as message_text,"
               "     extract(epoch from m.created_at) :: bigint as message_created_at,"
               "     extract(epoch from m.updated_at) :: bigint as message_updated_at,"
               // Channel fields
               "     ch.id as channel_id, ch.name as channel_name,"
               "     ch.content_type as channel_content_type, ch.max_size as channel_max_size,"
               "     extract(epoch from ch.created_at) :: bigint as channel_created_at,"
               "     extract(epoch from ch.updated_at) :: bigint as channel_updated_at"
               "     FROM disruption AS d"
               "     JOIN contributor AS co ON d.contributor_id = co.id"
               "     JOIN cause AS c ON (c.id = d.cause_id)"
               "     LEFT JOIN associate_disruption_tag ON associate_disruption_tag.disruption_id = d.id"
               "     LEFT JOIN tag AS t ON associate_disruption_tag.tag_id = t.id"
               "     JOIN impact AS i ON i.disruption_id = d.id"
               "     JOIN application_periods AS a ON a.impact_id = i.id"
               "     JOIN severity AS s ON s.id = i.severity_id"
               "     JOIN associate_impact_pt_object ON associate_impact_pt_object.impact_id = i.id"
               "     JOIN pt_object AS p ON associate_impact_pt_object.pt_object_id = p.id"
               "     LEFT JOIN line_section ON p.id = line_section.object_id"
               "     LEFT JOIN pt_object AS ls_line ON line_section.line_object_id = ls_line.id"
               "     LEFT JOIN pt_object AS ls_start ON line_section.start_object_id = ls_start.id"
               "     LEFT JOIN pt_object AS ls_end ON line_section.end_object_id = ls_end.id"
               "     LEFT JOIN message AS m ON m.impact_id = i.id"
               "     LEFT JOIN channel AS ch ON m.channel_id = ch.id"
               "     WHERE co.contributor_code = ANY('{%s}')" // it's like a "IN" but won't crash if empty
               "     AND d.status = 'published'"
               "     AND i.status = 'published'"
               "     ORDER BY d.id, c.id, t.id, i.id, a.id, p.id, m.id, ch.id"
               "     LIMIT %i OFFSET %i"
               " ;") % contributors_array % items_per_request % offset).str();

        result = work.exec(request);
        for (auto res : result) {
            reader(res);
        }

        offset += result.size();
    }while(result.size() > 0);
    reader.finalize();

    LOG4CPLUS_INFO(log4cplus::Logger::getInstance("Logger"),offset << " disruptions loaded");
}

void DisruptionDatabaseReader::finalize() {
    if (disruption && disruption->id() != "") {
        add_disruption(*disruption, pt_data, meta);
    }
}

}
