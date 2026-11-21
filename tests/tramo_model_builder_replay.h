#ifndef KALOS_TESTS_TRAMO_MODEL_BUILDER_REPLAY_H
#define KALOS_TESTS_TRAMO_MODEL_BUILDER_REPLAY_H

// Shared replay layer for the item 23-A corpus: the probe-line driver
// (parses the oracle protocol lines captured in inputs.csv into a
// kalos::tramo_build_request, mirroring TramoModelBuilderProbe.java line for
// line) plus the fact formatters that render a kalos::tramo_build_result
// back into the captured line grammar ("key value", %.17g doubles with the
// Java Formatter trailing-zero rule, enum constant names). Both
// tests/test_tramo_model_builder.cpp (generated golden header) and
// tools/verify_tramo_model_builder.cpp (raw csv corpus) drive the unit
// through this header, so the comparison against the JVM lines is exactly
// the same code in both gates.

#include <kalos/tramo_model_builder.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tmbreplay {

using kalos::tramo_build_request;
using kalos::tramo_build_result;
using kalos::tramo_parameter;
using kalos::tramo_sarima_spec;

// Java String.format("%.17g") for finite doubles: Java renders the SHORTEST
// round-tripping decimal and pads it to 17 significant digits (it does NOT
// expand the binary value), with the C %g threshold rules for scientific
// notation. C's printf("%#.17g") expands the exact binary value instead and
// differs for every double whose shortest form is shorter than 17 digits
// (0.7 -> Java "0.70000000000000000", C "0.69999999999999996"), so the
// shortest-decimal route is reproduced here.
inline std::string g17(double v) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    const bool negative = std::signbit(v);
    const double a = std::fabs(v);
    if (a == 0.0) return (negative ? "-0.0000000000000000" : "0.0000000000000000");
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof(buf), a, std::chars_format::scientific);
    std::string s(buf, static_cast<std::size_t>(res.ptr - buf));
    // s == "d[.ddd]e±X" (shortest). digits: point removed; X10 = value = 0.digits x 10^X10.
    const std::size_t epos = s.find('e');
    const int exp10 = std::stoi(s.substr(epos + 1));
    std::string digits;
    for (std::size_t i = 0; i < epos; ++i)
        if (s[i] != '.') digits += s[i];
    // strip no trailing zeros: to_chars shortest already has none.
    const int x = exp10 + 1;  // value = 0.digits x 10^x
    std::string out;
    if (x >= -3 && x < 17) {
        // fixed notation: 17 significant digits
        if (x <= 0) {
            out = "0.";
            for (int i = 0; i < -x; ++i) out += '0';
            out += digits;
            const int used = -x + static_cast<int>(digits.size());
            for (int i = used; i < 17; ++i) out += '0';
        } else {
            std::string intpart = digits.substr(0, std::min<std::size_t>(digits.size(), x));
            for (std::size_t i = intpart.size(); i < static_cast<std::size_t>(x); ++i)
                intpart += '0';
            const std::string fracpart =
                digits.size() > static_cast<std::size_t>(x) ? digits.substr(x) : std::string();
            out = intpart + "." + fracpart;
            for (std::size_t i = fracpart.size(); i < static_cast<std::size_t>(17 - x); ++i)
                out += '0';
        }
    } else {
        // scientific: d.dddddddddddddddd e±XX (17 significant digits, >=2 exp digits)
        out = digits.substr(0, 1) + ".";
        std::string frac = digits.substr(1);
        out += frac;
        for (std::size_t i = frac.size(); i < 16; ++i) out += '0';
        char esign = x - 1 >= 0 ? '+' : '-';
        const int e = std::abs(x - 1);
        char ebuf[16];
        std::snprintf(ebuf, sizeof(ebuf), "e%c%02d", esign, e);
        out += ebuf;
    }
    return negative ? "-" + out : out;
}

inline std::string yn(bool b) {
    return b ? "y" : "n";
}

inline std::string param_name(kalos::tramo_parameter_kind kind) {
    return kind == kalos::tramo_parameter_kind::fixed ? "F" : "R";
}

inline std::string fmt_param(const tramo_parameter& p) {
    return param_name(p.kind) + ":" + g17(p.value);
}

inline std::string fmt_params(const std::vector<tramo_parameter>& ps) {
    if (ps.empty()) return "-";
    std::string s;
    for (std::size_t i = 0; i < ps.size(); ++i) {
        if (i) s += ';';
        s += fmt_param(ps[i]);
    }
    return s;
}

inline std::string function_name(kalos::tramo_transform_function f) {
    switch (f) {
        case kalos::tramo_transform_function::none:
            return "None";
        case kalos::tramo_transform_function::automatic:
            return "Auto";
        case kalos::tramo_transform_function::log:
            return "Log";
    }
    return "?";
}

inline std::string lop_name(kalos::tramo_lop l) {
    switch (l) {
        case kalos::tramo_lop::none:
            return "None";
        case kalos::tramo_lop::leap_year:
            return "LeapYear";
        case kalos::tramo_lop::length_of_period:
            return "LengthOfPeriod";
    }
    return "?";
}

inline std::string td_type_name(kalos::tramo_trading_days_type t) {
    switch (t) {
        case kalos::tramo_trading_days_type::none:
            return "NONE";
        case kalos::tramo_trading_days_type::td2:
            return "TD2";
        case kalos::tramo_trading_days_type::td3:
            return "TD3";
        case kalos::tramo_trading_days_type::td4:
            return "TD4";
        case kalos::tramo_trading_days_type::td6:
            return "TD6";
        case kalos::tramo_trading_days_type::td7:
            return "TD7";
        case kalos::tramo_trading_days_type::working_days:
            return "WorkingDays";
    }
    return "?";
}

inline std::string auto_method_name(kalos::tramo_auto_method m) {
    switch (m) {
        case kalos::tramo_auto_method::unused:
            return "UNUSED";
        case kalos::tramo_auto_method::ftest:
            return "FTEST";
        case kalos::tramo_auto_method::wald:
            return "WALD";
        case kalos::tramo_auto_method::bic:
            return "BIC";
        case kalos::tramo_auto_method::aic:
            return "AIC";
    }
    return "?";
}

inline std::string easter_type_name(kalos::tramo_easter_type t) {
    switch (t) {
        case kalos::tramo_easter_type::unused:
            return "Unused";
        case kalos::tramo_easter_type::standard:
            return "Standard";
        case kalos::tramo_easter_type::include_easter:
            return "IncludeEaster";
        case kalos::tramo_easter_type::include_easter_monday:
            return "IncludeEasterMonday";
    }
    return "?";
}

inline std::string fmt_ym(int year, int month) {
    return std::to_string(year) + ":" + std::to_string(month);
}

inline std::string fmt_domain(int year, int position, int freq, int length) {
    return std::to_string(year) + ":" + std::to_string(position) + ":" +
           std::to_string(freq) + ":" + std::to_string(length);
}

inline std::string fmt_sarima(const tramo_sarima_spec& a) {
    std::string s = "p" + std::to_string(a.phi.size()) + "d" + std::to_string(a.d) + "q" +
                    std::to_string(a.theta.size()) + "P" + std::to_string(a.bphi.size()) + "D" +
                    std::to_string(a.bd) + "Q" + std::to_string(a.btheta.size()) + " per" +
                    std::to_string(a.period) + " und" + yn(a.is_undefined()) + " air" +
                    yn(a.is_airline()) + " phi[" + fmt_params(a.phi) + "] theta[" +
                    fmt_params(a.theta) + "] bphi[" + fmt_params(a.bphi) + "] btheta[" +
                    fmt_params(a.btheta) + "]";
    return s;
}

inline std::string kind_name(kalos::tramo_variable_kind k) {
    using kalos::tramo_variable_kind;
    switch (k) {
        case tramo_variable_kind::trend_constant:
            return "TrendConstant";
        case tramo_variable_kind::additive_outlier:
            return "AdditiveOutlier";
        case tramo_variable_kind::level_shift:
            return "LevelShift";
        case tramo_variable_kind::transitory_change:
            return "TransitoryChange";
        case tramo_variable_kind::periodic_outlier:
            return "PeriodicOutlier";
        case tramo_variable_kind::trading_days:
            return "GenericTradingDaysVariable";
        case tramo_variable_kind::stock_trading_days:
            return "StockTradingDays";
        case tramo_variable_kind::length_of_period:
            return "LengthOfPeriod";
        case tramo_variable_kind::easter_variable:
            return "EasterVariable";
        case tramo_variable_kind::julian_easter_variable:
            return "JulianEasterVariable";
        case tramo_variable_kind::user_variable:
            return "TsContextVariable";
        case tramo_variable_kind::intervention:
            return "InterventionVariable";
        case tramo_variable_kind::ramp:
            return "Ramp";
    }
    return "?";
}

inline std::string fmt_info(const kalos::tramo_model_variable& v) {
    using kalos::tramo_variable_kind;
    switch (v.kind) {
        case tramo_variable_kind::trend_constant:
            return "d" + std::to_string(v.constant_d) + " bd" + std::to_string(v.constant_bd) +
                   " ref" + fmt_ym(v.constant_ref_year, v.constant_ref_month);
        case tramo_variable_kind::additive_outlier:
        case tramo_variable_kind::level_shift:
        case tramo_variable_kind::transitory_change:
        case tramo_variable_kind::periodic_outlier: {
            std::string s = "code" + v.outlier_code + " pos" +
                            fmt_ym(v.position_year, v.position_month);
            if (v.kind == tramo_variable_kind::periodic_outlier)
                s += " per" + std::to_string(v.period);
            return s;
        }
        case tramo_variable_kind::ramp:
            return "start" + fmt_ym(v.start_year, v.start_month) + " end" +
                   fmt_ym(v.end_year, v.end_month);
        case tramo_variable_kind::intervention: {
            std::string s = "delta" + g17(v.delta) + " dseason" + g17(v.delta_seasonal) + " seq[";
            for (std::size_t i = 0; i < v.sequences.size(); ++i) {
                if (i) s += ',';
                s += fmt_ym(v.sequences[i].first.first, v.sequences[i].first.second) + "-" +
                     fmt_ym(v.sequences[i].second.first, v.sequences[i].second.second);
            }
            return s + "]";
        }
        case tramo_variable_kind::trading_days:
            return "type" + td_type_name(v.td_type) + " groups" + std::to_string(v.td_groups);
        case tramo_variable_kind::length_of_period:
            return "lop" + lop_name(v.lop);
        case tramo_variable_kind::easter_variable:
            return "dur" + std::to_string(v.easter_duration) + " end" +
                   std::to_string(v.easter_end_position) + " meanSimple";
        case tramo_variable_kind::julian_easter_variable:
            return "dur" + std::to_string(v.easter_duration) + " gregorian" +
                   yn(v.easter_gregorian);
        case tramo_variable_kind::user_variable:
        case tramo_variable_kind::stock_trading_days:
            return "";
    }
    return "";
}

// ---- the probe-line driver -------------------------------------------------

struct parsed_case {
    tramo_build_request req;
    std::string template_name;
    std::string id;
};

inline std::vector<std::string> split_ws(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

inline tramo_parameter parse_param_token(const std::string& tok) {
    if (tok == "U") return tramo_parameter{};
    if (tok == "R") {
        tramo_parameter p;
        p.kind = kalos::tramo_parameter_kind::estimated;
        return p;
    }
    if (tok.rfind("F:", 0) != 0) throw std::runtime_error("bad parameter token: " + tok);
    tramo_parameter p;
    p.kind = kalos::tramo_parameter_kind::fixed;
    p.value = std::strtod(tok.c_str() + 2, nullptr);
    return p;
}

inline kalos::tramo_transform_function parse_function(const std::string& v) {
    if (v == "None") return kalos::tramo_transform_function::none;
    if (v == "Auto") return kalos::tramo_transform_function::automatic;
    if (v == "Log") return kalos::tramo_transform_function::log;
    throw std::runtime_error("bad function: " + v);
}

inline kalos::tramo_lop parse_lop(const std::string& v) {
    if (v == "None") return kalos::tramo_lop::none;
    if (v == "LeapYear") return kalos::tramo_lop::leap_year;
    if (v == "LengthOfPeriod") return kalos::tramo_lop::length_of_period;
    throw std::runtime_error("bad lop: " + v);
}

inline parsed_case parse_case(const std::vector<std::string>& lines) {
    parsed_case pc;
    bool calendar_touched = false;
    bool transform_touched = false;
    for (const std::string& line0 : lines) {
        std::string line = line0;
        if (line.rfind("src ", 0) == 0) line = line.substr(4);  // captured-line prefix
        std::vector<std::string> t = split_ws(line);
        if (t.empty()) continue;
        const std::string& key = t[0];
        if (key == "id") {
            pc.id = t[1];
        } else if (key == "freq") {
            pc.req.annual_frequency = std::stoi(t[1]);
        } else if (key == "start") {
            pc.req.start_year = std::stoi(t[1]);
            pc.req.start_month = std::stoi(t[2]);
        } else if (key == "n" || key == "nan") {
            // geometry is carried by the golden values array; no effect here
        } else if (key == "template") {
            pc.template_name = t[1];
            const kalos::tramo_template_fields* f = kalos::tramo_template(t[1]);
            if (f == nullptr) throw std::runtime_error("unknown template: " + t[1]);
            pc.req.using_auto_model = f->using_auto_model;
            pc.req.transform_function = f->transform_function;
            pc.req.transform_adjust = f->transform_adjust;
            pc.req.arima = f->arima;
            pc.req.mean = f->mean;
            pc.req.trading_days = f->trading_days;
            pc.req.easter = f->easter;
            pc.req.delta_tc = f->delta_tc;
        } else if (key == "automodel") {
            pc.req.using_auto_model = t[1] == "on";
        } else if (key == "arima") {
            const std::size_t p = std::stoul(t[1]), q = std::stoul(t[3]);
            const std::size_t bp = std::stoul(t[4]), bq = std::stoul(t[6]);
            std::size_t k = 7;
            tramo_sarima_spec a;
            a.d = std::stoi(t[2]);
            a.bd = std::stoi(t[5]);
            for (std::size_t i = 0; i < p; ++i) a.phi.push_back(parse_param_token(t[k++]));
            for (std::size_t i = 0; i < q; ++i) a.theta.push_back(parse_param_token(t[k++]));
            for (std::size_t i = 0; i < bp; ++i) a.bphi.push_back(parse_param_token(t[k++]));
            for (std::size_t i = 0; i < bq; ++i) a.btheta.push_back(parse_param_token(t[k++]));
            pc.req.arima = a;  // builder default period 0
        } else if (key == "function" || key == "adjust") {
            // Pinned probe semantics: the knob lines rebuild the TransformSpec
            // from the builder defaults (the template transform is replaced,
            // the other knob falls back to its default).
            if (!transform_touched) {
                transform_touched = true;
                pc.req.transform_function = kalos::tramo_transform_function::none;
                pc.req.transform_adjust = kalos::tramo_lop::none;
            }
            if (key == "function") pc.req.transform_function = parse_function(t[1]);
            else pc.req.transform_adjust = parse_lop(t[1]);
        } else if (key == "mean") {
            const std::string v = t[1];
            if (v == "none") {
                pc.req.mean = kalos::tramo_mean_spec{};
            } else if (v == "used") {
                pc.req.mean = kalos::tramo_mean_spec{true, true, false, true, tramo_parameter{}};
            } else if (v.rfind("fixed:", 0) == 0) {
                tramo_parameter c;
                c.kind = kalos::tramo_parameter_kind::fixed;
                c.value = std::strtod(v.c_str() + 6, nullptr);
                pc.req.mean = kalos::tramo_mean_spec{true, true, false, true, c};
            } else {
                throw std::runtime_error("bad mean: " + v);
            }
        } else if (key == "td" || key == "tdauto" || key == "tdstock") {
            if (!calendar_touched) {
                calendar_touched = true;
                pc.req.trading_days = kalos::tramo_trading_days_spec{};
                pc.req.easter = kalos::tramo_easter_spec{};
            }
            if (key == "td") {
                const std::string type = t[1], lop = t[2], test = t[3];
                auto& td = pc.req.trading_days;
                if (type == "TD2") td.type = kalos::tramo_trading_days_type::td2;
                else if (type == "TD3") td.type = kalos::tramo_trading_days_type::td3;
                else if (type == "TD4") td.type = kalos::tramo_trading_days_type::td4;
                else if (type == "TD6") td.type = kalos::tramo_trading_days_type::td6;
                else if (type == "TD7") td.type = kalos::tramo_trading_days_type::td7;
                else if (type == "WorkingDays") td.type = kalos::tramo_trading_days_type::working_days;
                else td.type = kalos::tramo_trading_days_type::none;
                td.lop = parse_lop(lop);
                td.test = test != "None";
                td.used = !(td.type == kalos::tramo_trading_days_type::none &&
                            td.lop == kalos::tramo_lop::none && !td.test);
            } else {
                throw std::runtime_error("calendar line outside the corpus: " + key);
            }
        } else if (key == "easter") {
            if (!calendar_touched) {
                calendar_touched = true;
                pc.req.trading_days = kalos::tramo_trading_days_spec{};
                pc.req.easter = kalos::tramo_easter_spec{};
            }
            const std::string type = t[1];
            auto& e = pc.req.easter;
            e.type = type == "Standard"             ? kalos::tramo_easter_type::standard
                     : type == "IncludeEaster"      ? kalos::tramo_easter_type::include_easter
                     : type == "IncludeEasterMonday"? kalos::tramo_easter_type::include_easter_monday
                                                    : kalos::tramo_easter_type::unused;
            e.used = e.type != kalos::tramo_easter_type::unused;
            e.test = t[2] == "1";
            e.duration = std::stoi(t[3]);
            e.julian = t[4] == "1";
        } else if (key == "outlier") {
            kalos::tramo_outlier_request o;
            o.code = t[1];
            const std::string pos = t[2];
            const std::size_t colon = pos.find(':');
            o.position_year = std::stoi(pos.substr(0, colon));
            o.position_month = std::stoi(pos.substr(colon + 1));
            std::string mname = pos;
            for (auto& c : mname)
                if (c == ':') c = 'M';
            o.name = pos + "." + o.code + "." + mname;
            for (std::size_t i = 3; i < t.size(); ++i) {
                if (t[i].rfind("regeffect=", 0) == 0) {
                    o.has_regeffect_override = true;
                    o.regeffect_override = t[i].substr(10);
                }
            }
            pc.req.outliers.push_back(std::move(o));
        } else if (key == "user") {
            pc.req.user_variables.push_back(kalos::tramo_user_variable_request{t[1], false});
        } else if (key == "ramp") {
            kalos::tramo_ramp_request r;
            r.name = "RAMP";
            const auto sp = [](const std::string& s) {
                const std::size_t c = s.find(':');
                return std::make_pair(std::stoi(s.substr(0, c)), std::stoi(s.substr(c + 1)));
            };
            auto a = sp(t[1]);
            auto b = sp(t[2]);
            r.start_year = a.first;
            r.start_month = a.second;
            r.end_year = b.first;
            r.end_month = b.second;
            pc.req.ramps.push_back(std::move(r));
        } else if (key == "intervention") {
            kalos::tramo_intervention_request v;
            v.name = "IV";
            const auto sp = [](const std::string& s) {
                const std::size_t c = s.find(':');
                return std::make_pair(std::stoi(s.substr(0, c)), std::stoi(s.substr(c + 1)));
            };
            v.sequences.emplace_back(sp(t[1]), sp(t[2]));
            pc.req.interventions.push_back(std::move(v));
        } else {
            throw std::runtime_error("unknown protocol line: " + line);
        }
    }
    return pc;
}

// ---- line formatters --------------------------------------------------------

inline std::vector<std::string> format_req(const parsed_case& pc, bool regression_all_spans = true) {
    const tramo_build_request& r = pc.req;
    std::vector<std::string> out;
    out.push_back("req template " +
                  (pc.template_name.empty() ? std::string("null") : pc.template_name));
    out.push_back("req auto " + yn(r.using_auto_model));
    out.push_back("req arima " + fmt_sarima(r.arima));
    out.push_back("req function " + function_name(r.transform_function) + " adjust " +
                  lop_name(r.transform_adjust) +
                  " tspan all=" + yn(regression_all_spans));
    out.push_back("req espan all=" + yn(regression_all_spans));
    out.push_back("req mean used=" + yn(r.mean.used) + " tconst=" + yn(r.mean.trend_constant) +
                  " test=" + yn(r.mean.test) + " coef " +
                  (r.mean.coefficient_present ? fmt_param(r.mean.coefficient)
                                              : std::string("null")));
    // The outlier flags are template data; the driver only carries delta_tc.
    // For request echoes the flags come from the template row itself.
    const kalos::tramo_template_fields* f =
        pc.template_name.empty() ? nullptr : kalos::tramo_template(pc.template_name);
    const bool ao = f ? f->outliers_ao : false;
    const bool ls = f ? f->outliers_ls : false;
    const bool tc = f ? f->outliers_tc : false;
    const bool so = f ? f->outliers_so : false;
    out.push_back("req ospec ao=" + yn(ao) + " ls=" + yn(ls) + " tc=" + yn(tc) + " so=" + yn(so) +
                  " deltatc " + g17(r.delta_tc));
    out.push_back("req td used=" + yn(r.trading_days.used) + " test=" + yn(r.trading_days.test) +
                  " auto=" + yn(r.trading_days.automatic) + " stock=" + yn(r.trading_days.stock) +
                  " type " + td_type_name(r.trading_days.type) + " autoM " +
                  auto_method_name(r.trading_days.auto_method) + " stockk " +
                  std::to_string(r.trading_days.stock_days) + " lop " +
                  lop_name(r.trading_days.lop));
    out.push_back("req easter used=" + yn(r.easter.used) + " test=" + yn(r.easter.test) + " type " +
                  easter_type_name(r.easter.type) + " dur " + std::to_string(r.easter.duration) +
                  " julian " + yn(r.easter.julian));
    out.push_back("req counts outliers " + std::to_string(r.outliers.size()) + " users " +
                  std::to_string(r.user_variables.size()) + " ramps " +
                  std::to_string(r.ramps.size()) + " ivars " +
                  std::to_string(r.interventions.size()) + " regused " +
                  yn(kalos::tramo_regression_used(r)));
    return out;
}

// "key value" lines in the captured order (status ... nvar, then var lines).
inline std::vector<std::string> format_facts(const tramo_build_result& b) {
    std::vector<std::string> out;
    out.push_back("status ok");
    out.push_back("afreq " + std::to_string(b.annual_frequency));
    out.push_back("domain " + fmt_domain(b.domain_year, b.domain_position, b.annual_frequency,
                                         b.domain_length));
    out.push_back("estdomain " + fmt_domain(b.estimation_year, b.estimation_position,
                                            b.annual_frequency, b.estimation_length));
    if (b.missing.empty()) {
        out.push_back("missing none");
    } else {
        std::string m = "missing ", iv = "interp ";
        for (std::size_t i = 0; i < b.missing.size(); ++i) {
            if (i) {
                m += ';';
                iv += ';';
            }
            m += std::to_string(b.missing[i]);
            iv += std::to_string(b.missing[i]) + "=" +
                  g17(b.interpolated[static_cast<std::size_t>(b.missing[i])]);
        }
        out.push_back(m);
        out.push_back(iv);
    }
    out.push_back("log " + yn(b.log_transformation) + " preadj " + lop_name(b.preadjustment) +
                  " mean " + yn(b.mean));
    out.push_back("arima " + fmt_sarima(b.arima));
    out.push_back("nvar " + std::to_string(b.variables.size()));
    for (std::size_t i = 0; i < b.variables.size(); ++i) {
        const auto& v = b.variables[i];
        out.push_back("var " + std::to_string(i) + " name=" + v.name + " kind=" + kind_name(v.kind) +
                      " dim=" + std::to_string(v.dim) + " regeffect=" +
                      (v.has_regeffect ? v.regeffect : std::string("-")) + " ami=" + yn(v.has_ami) +
                      " coef=" + fmt_params(v.coefficients) + " info=" + fmt_info(v));
    }
    return out;
}

// "kind message" in emission order (the builder's ProcessingLog trace; the
// envelope produces warnings only).
inline std::vector<std::string> format_events(const tramo_build_result& b) {
    std::vector<std::string> out;
    for (const auto& w : b.warnings) out.push_back("warn " + w);
    return out;
}

// ---- small csv line reader (double-quoted fields, "" escapes) ---------------

inline std::vector<std::string> csv_fields(const std::string& line) {
    std::vector<std::string> out;
    std::string field;
    bool in_quotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += c;
            }
        } else if (c == '"') {
            in_quotes = true;
        } else if (c == ',') {
            out.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    out.push_back(field);
    return out;
}

}  // namespace tmbreplay

#endif
