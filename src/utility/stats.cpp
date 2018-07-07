/*
    This file is a part of SORT(Simple Open Ray Tracing), an open-source cross
    platform physically based renderer.
 
    Copyright (c) 2011-2018 by Cao Jiayin - All rights reserved.
 
    SORT is a free software written for educational purpose. Anyone can distribute
    or modify it under the the terms of the GNU General Public License Version 3 as
    published by the Free Software Foundation. However, there is NO warranty that
    all components are functional in a perfect manner. Without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.
 
    You should have received a copy of the GNU General Public License along with
    this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.html>.
 */

#include "stats.h"
#include "utility/sassert.h"
#include "utility/strhelper.h"

#ifdef SORT_ENABLE_STATS_COLLECTION
// This is a container holding all statsItem per thread
class StatsItemContainer {
public:
    void Register(const StatsItemRegister* item) {
        container.push_back(item);
    }
    void FlushData() {
        sAssert(!flushed, LOG_GENERAL);
        for (const StatsItemRegister* item : container)
            item->FlushData();
        flushed = true;
    }

private:
    // Container for all stats per thread
    std::vector<const StatsItemRegister*> container;
    // To make sure we don't flush data twice
    bool flushed = false;
};

void StatsSummary::PrintStats() const {
    slog(INFO, GENERAL, "-------------------------Statistics-------------------------");
    std::map<std::string, std::map<std::string, std::string>> outputs;
    for (const auto& counterCat : counters) {
        for (const auto& counterItem : counterCat.second) {
            outputs[counterCat.first][counterItem.first] = counterItem.second->ToString();
        }
    }

    for (const auto& counterCat : outputs) {
        slog(INFO, GENERAL, stringFormat("%s", counterCat.first.c_str()));
            for (const auto& counterItem : counterCat.second) {
                slog(INFO, GENERAL, stringFormat("    %-38s %s", counterItem.first.c_str() , counterItem.second.c_str()));
            }
    }
    slog(INFO, GENERAL, "-------------------------Statistics-------------------------");
}

static StatsSummary                     g_StatsSummary;
static Thread_Local StatsItemContainer  g_StatsItemContainer;

// Recording all necessary data in constructor
StatsItemRegister::StatsItemRegister( const stats_update f ): func(f) 
{
    static std::mutex statsMutex;
    std::lock_guard<std::mutex> lock(statsMutex);
    g_StatsItemContainer.Register(this);
}

// Flush the data into StatsSummary
void StatsItemRegister::FlushData() const
{
    sAssert(func, LOG_GENERAL);
    func(g_StatsSummary);
}

std::string StatsInt::ToString(long long v){
    auto s = to_string(v);
    if( s.size() < 5 )
        return s;
    int len = (int)s.size() - 1;
    std::string ret( len + 1 + len / 3 , ',' );
    int i = 0 , j = (int)ret.size() - 1;
    while( i < s.size() ){
        ret[j--] = s[len - (i++)];
        if( i % 3 == 0 )
            --j;
    }
    return ret;
}

std::string StatsElaspedTime::ToString( long long v ){
    if( v < 1000 ) return stringFormat("%d(ms)" , v);
    if( v < 60000 ) return stringFormat("%.2f(s)" , (float)v/1000.0f); v /= 1000.0f;
    if( v < 3600 ) return stringFormat( "%d(m)%d(s)" , v/60 , v%60 ); v /= 60.0f;
    if( v < 1440 ) return stringFormat( "%d(h)%d(m)" , v/60 , v%60 );
    return stringFormat( "%d(d)%d(h)%d(m)" , v / 1440 , ( v % 1440 ) / 60 , v % 60 );
}

std::string StatsFloat::ToString( float v ){
    return stringFormat("%.2f",v);
}

std::string StatsRatio::ToString( StatsData_Ratio ratio ){
    return stringFormat("%d/%d",ratio.nominator,ratio.denominator);
}

#endif

void FlushStatsData()
{
    SORT_STATS(g_StatsItemContainer.FlushData());
}
void PrintStatsData()
{
    SORT_STATS(g_StatsSummary.PrintStats());
}
