#include "htmlrenderer.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <libgen.h>
#include <sstream>
#include <stdexcept>

#include "config.h"
#include "logger.h"
#include "strprintf.h"
#include "tagsouppullparser.h"
#include "utils.h"

namespace newsboat {

HtmlRenderer::HtmlRenderer(bool raw)
	: raw_(raw)
{
	tags["a"] = HtmlTag::A;
	tags["embed"] = HtmlTag::EMBED;
	tags["br"] = HtmlTag::BR;
	tags["pre"] = HtmlTag::PRE;
	tags["ituneshack"] = HtmlTag::ITUNESHACK;
	tags["img"] = HtmlTag::IMG;
	tags["blockquote"] = HtmlTag::BLOCKQUOTE;
	tags["aside"] = HtmlTag::BLOCKQUOTE;
	tags["p"] = HtmlTag::P;
	tags["h1"] = HtmlTag::H1;
	tags["h2"] = HtmlTag::H2;
	tags["h3"] = HtmlTag::H3;
	tags["h4"] = HtmlTag::H4;
	tags["h5"] = HtmlTag::H5;
	tags["h6"] = HtmlTag::H6;
	tags["ol"] = HtmlTag::OL;
	tags["ul"] = HtmlTag::UL;
	tags["li"] = HtmlTag::LI;
	tags["dt"] = HtmlTag::DT;
	tags["dd"] = HtmlTag::DD;
	tags["dl"] = HtmlTag::DL;
	tags["sup"] = HtmlTag::SUP;
	tags["sub"] = HtmlTag::SUB;
	tags["hr"] = HtmlTag::HR;
	tags["b"] = HtmlTag::STRONG;
	tags["strong"] = HtmlTag::STRONG;
	tags["u"] = HtmlTag::UNDERLINE;
	tags["q"] = HtmlTag::QUOTATION;
	tags["script"] = HtmlTag::SCRIPT;
	tags["style"] = HtmlTag::STYLE;
	tags["table"] = HtmlTag::TABLE;
	tags["th"] = HtmlTag::TH;
	tags["tr"] = HtmlTag::TR;
	tags["td"] = HtmlTag::TD;
	tags["video"] = HtmlTag::VIDEO;
	tags["audio"] = HtmlTag::AUDIO;
	tags["source"] = HtmlTag::SOURCE;
}

void HtmlRenderer::render(const std::string& source,
	std::vector<std::pair<LineType, std::string>>& lines,
	std::vector<LinkPair>& links,
	const std::string& url)
{
	std::istringstream input(source);
	render(input, lines, links, url);
}

unsigned int HtmlRenderer::add_link(std::vector<LinkPair>& links,
	const std::string& link,
	LinkType type)
{
	bool found = false;
	unsigned int i = 1;
	for (const auto& l : links) {
		if (l.first == link) {
			found = true;
			break;
		}
		i++;
	}
	if (!found) {
		links.push_back(LinkPair(link, type));
	}

	return i;
}

void HtmlRenderer::render(std::istream& input,
	std::vector<std::pair<LineType, std::string>>& lines,
	std::vector<LinkPair>& links,
	const std::string& url)
{
	unsigned int image_count = 0;
	unsigned int video_count = 0;
	unsigned int audio_count = 0;
	unsigned int source_count = 0;
	std::string curline;
	int indent_level = 0;
	std::vector<HtmlTag> list_elements_stack;
	bool inside_pre = false;
	bool itunes_hack = false;
	size_t inside_script = 0;
	size_t inside_style = 0;
	bool inside_video = false;
	bool inside_audio = false;
	std::vector<unsigned int> ol_counts;
	std::vector<char> ol_types;
	HtmlTag current_tag;
	int link_num = -1;
	std::vector<Table> tables;

	/*
	 * to render the HTML, we use a self-developed "XML" pull parser.
	 *
	 * A pull parser works like this:
	 *   - we feed it with an XML stream
	 *   - we then gather an iterator
	 *   - we then can iterate over all continuous elements, such as start
	 * tag, close tag, text element, ...
	 */
	TagSoupPullParser xpp;
	xpp.set_input(input);

	for (TagSoupPullParser::Event e = xpp.next();
		e != TagSoupPullParser::Event::END_DOCUMENT;
		e = xpp.next()) {
		std::string tagname;
		switch (e) {
		case TagSoupPullParser::Event::START_TAG:
			tagname = xpp.get_text();
			std::transform(tagname.begin(),
				tagname.end(),
				tagname.begin(),
				::tolower);
			current_tag = tags[tagname];

			switch (current_tag) {
			case HtmlTag::A: {
				std::string link;
				try {
					link = xpp.get_attribute_value("href");
				} catch (const std::invalid_argument&) {
					LOG(Level::WARN,
						"HtmlRenderer::render: found a "
						"tag "
						"with no href attribute");
					link = "";
				}
				if (link.length() > 0) {
					link_num = add_link(links,
							utils::censor_url(
								utils::absolute_url(
									url, link)),
							LinkType::HREF);
					if (!raw_) {
						curline.append("<u>");
					}
				}
			}
			break;
			case HtmlTag::STRONG:
				if (!raw_) {
					curline.append("<b>");
				}
				break;
			case HtmlTag::UNDERLINE:
				if (!raw_) {
					curline.append("<u>");
				}
				break;
			case HtmlTag::QUOTATION:
				if (!raw_) {
					curline.append("\"");
				}
				break;

			case HtmlTag::EMBED: {
				std::string type;
				try {
					type = xpp.get_attribute_value("type");
				} catch (const std::invalid_argument&) {
					LOG(Level::WARN,
						"HtmlRenderer::render: found "
						"embed "
						"object without type "
						"attribute");
					type = "";
				}
				if (type == "application/x-shockwave-flash") {
					std::string link;
					try {
						link = xpp.get_attribute_value(
								"src");
					} catch (const std::invalid_argument&) {
						LOG(Level::WARN,
							"HtmlRenderer::render: "
							"found embed object "
							"without src "
							"attribute");
						link = "";
					}
					if (link.length() > 0) {
						link_num = add_link(links,
								utils::censor_url(
									utils::absolute_url(
										url,
										link)),
								LinkType::EMBED);
						curline.append(strprintf::fmt(
								"[%s %u]",
								_("embedded flash:"),
								link_num));
					}
				}
			}
			break;

			case HtmlTag::BR:
				add_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::PRE:
				inside_pre = true;
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::ITUNESHACK:
				itunes_hack = true;
				break;

			case HtmlTag::IMG: {
				std::string imgurl;
				std::string imgtitle;
				try {
					imgurl = xpp.get_attribute_value("src");
				} catch (const std::invalid_argument&) {
					LOG(Level::WARN,
						"HtmlRenderer::render: found "
						"img "
						"tag with no src attribute");
					imgurl = "";
				}
				try {
					imgtitle = xpp.get_attribute_value(
							"title");
				} catch (const std::invalid_argument&) {
					imgtitle = "";
				}
				if (imgurl.length() > 0) {
					if (imgurl.substr(0, 5) == "data:") {
						link_num = add_link(links,
								"inline image",
								LinkType::IMG);
					} else {
						link_num = add_link(links,
								utils::censor_url(
									utils::absolute_url(
										url,
										imgurl)),
								LinkType::IMG);
					}
					if (imgtitle != "") {
						curline.append(strprintf::fmt(
								"[%s %u: %s]",
								_("image"),
								link_num,
								imgtitle));
					} else {
						curline.append(strprintf::fmt(
								"[%s %u]",
								_("image"),
								link_num));
					}
					image_count++;
				}
			}
			break;

			case HtmlTag::BLOCKQUOTE:
				++indent_level;
				add_nonempty_line(curline, tables, lines);
				add_line("", tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::H1:
			case HtmlTag::H2:
			case HtmlTag::H3:
			case HtmlTag::H4:
			case HtmlTag::H5:
			case HtmlTag::H6:
			case HtmlTag::P: {
				add_nonempty_line(curline, tables, lines);
				if (lines.size() > 0) {
					std::string::size_type last_line_len =
						lines[lines.size() - 1]
						.second.length();
					if (last_line_len >
						static_cast<unsigned int>(
							indent_level * 2)) {
						add_line("", tables, lines);
					}
				}
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
			}
			break;

			case HtmlTag::OL:
				list_elements_stack.push_back(HtmlTag::OL);
				{
					unsigned int ol_count = 1;
					std::string ol_count_str;
					try {
						ol_count_str =
							xpp.get_attribute_value(
								"start");
					} catch (const std::invalid_argument&) {
						ol_count_str = "1";
					}
					ol_count = utils::to_u(ol_count_str, 1);
					ol_counts.push_back(ol_count);

					std::string ol_type;
					try {
						ol_type =
							xpp.get_attribute_value(
								"type");
						if (ol_type != "1" &&
							ol_type != "a" &&
							ol_type != "A" &&
							ol_type != "i" &&
							ol_type != "I") {
							ol_type = "1";
						}
					} catch (const std::invalid_argument&) {
						ol_type = "1";
					}
					ol_types.push_back(ol_type[0]);
				}
				add_nonempty_line(curline, tables, lines);
				add_line("", tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::UL:
				list_elements_stack.push_back(HtmlTag::UL);
				add_nonempty_line(curline, tables, lines);
				add_line("", tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::LI: {
				if (list_elements_stack.size() >= 1
					&& list_elements_stack.back() == HtmlTag::LI) {
					list_elements_stack.pop_back();
					indent_level -= 2;
					if (indent_level < 0) {
						indent_level = 0;
					}
					add_nonempty_line(
						curline, tables, lines);
					prepare_new_line(curline,
						tables.size() ? 0
						: indent_level);
				}
				list_elements_stack.push_back(HtmlTag::LI);
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				indent_level += 2;

				const auto latest_list = std::find_if(list_elements_stack.rbegin(),
				list_elements_stack.rend(), [](const HtmlTag& tag) {
					return (tag == HtmlTag::OL || tag == HtmlTag::UL);
				});
				bool inside_ordered_list = false;
				if (latest_list != list_elements_stack.rend() && *latest_list == HtmlTag::OL) {
					inside_ordered_list = true;
				}
				if (inside_ordered_list && ol_counts.size() != 0) {
					curline.append(strprintf::fmt("%s. ",
							format_ol_count(
								ol_counts[ol_counts
									.size() -
									1],
								ol_types[ol_types.size() -
											1])));
					++ol_counts[ol_counts.size() - 1];
				} else {
					curline.append("  * ");
				}
			}
			break;

			case HtmlTag::DT:
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::DD:
				indent_level += 4;
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::DL:
				// ignore tag
				break;

			case HtmlTag::SUP:
				curline.append("^");
				break;

			case HtmlTag::SUB:
				curline.append("[");
				break;

			case HtmlTag::HR:
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				add_hr(lines);
				break;

			case HtmlTag::SCRIPT:
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);

				// don't render scripts, ignore current line
				inside_script++;
				break;

			case HtmlTag::STYLE:
				inside_style++;
				break;

			case HtmlTag::TABLE: {
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(
					curline, 0); // no indent in tables

				bool has_border = false;
				try {
					std::string b = xpp.get_attribute_value(
							"border");
					has_border = (utils::to_u(b, 0) > 0);
				} catch (const std::invalid_argument&) {
					// is ok, no border then
				}
				tables.push_back(Table(has_border));
				break;
			}

			case HtmlTag::TR:
				if (!tables.empty()) {
					tables.back().start_row();
				}
				break;

			case HtmlTag::TH: {
				size_t span = 1;
				try {
					span = utils::to_u(
							xpp.get_attribute_value(
								"colspan"),
							1);
				} catch (const std::invalid_argument&) {
					// is ok, span 1 then
				}
				if (!tables.empty()) {
					tables.back().start_cell(span);
				}
				curline.append("<b>");
				break;
			}

			case HtmlTag::TD: {
				size_t span = 1;
				try {
					span = utils::to_u(
							xpp.get_attribute_value(
								"colspan"),
							1);
				} catch (const std::invalid_argument&) {
					// is ok, span 1 then
				}
				if (!tables.empty()) {
					tables.back().start_cell(span);
				}
				break;
			}

			case HtmlTag::VIDEO: {
				std::string videourl;

				if (inside_video && source_count == 0) {
					video_count--;
				}

				if (inside_audio && source_count == 0) {
					audio_count--;
				}

				if (inside_video || inside_audio) {
					source_count = 0;
					inside_video = false;
					inside_audio = false;
					LOG(Level::WARN,
						"HtmlRenderer::render media element left unclosed");
				}

				try {
					videourl = xpp.get_attribute_value("src");
					source_count++;
				} catch (const std::invalid_argument&) {
					videourl = "";
				}

				video_count++;
				inside_video = true;
				add_media_link(curline, links, url, videourl,
					video_count, LinkType::VIDEO);
			}
			break;

			case HtmlTag::AUDIO: {
				std::string audiourl;

				if (inside_video && source_count == 0) {
					video_count--;
				}

				if (inside_audio && source_count == 0) {
					audio_count--;
				}

				if (inside_video || inside_audio) {
					source_count = 0;
					inside_video = false;
					inside_audio = false;
					LOG(Level::WARN,
						"HtmlRenderer::render media element left unclosed");
				}

				try {
					audiourl = xpp.get_attribute_value("src");
					source_count++;
				} catch (const std::invalid_argument&) {
					audiourl = "";
				}

				audio_count++;
				inside_audio = true;
				add_media_link(curline, links, url, audiourl,
					audio_count, LinkType::AUDIO);
			}
			break;

			case HtmlTag::SOURCE: {
				std::string sourceurl;

				try {
					sourceurl = xpp.get_attribute_value("src");
					source_count++;
				} catch (const std::invalid_argument&) {
					LOG(Level::WARN,
						"HtmlRenderer::render: found source tag with no src attribute");
					sourceurl = "";
				}

				if (inside_video) {
					add_media_link(curline, links, url,
						sourceurl, video_count,
						LinkType::VIDEO);
				}

				if (inside_audio) {
					add_media_link(curline, links, url,
						sourceurl, audio_count,
						LinkType::AUDIO);
				}
			}
			}
			break;

		case TagSoupPullParser::Event::END_TAG:
			tagname = xpp.get_text();
			std::transform(tagname.begin(),
				tagname.end(),
				tagname.begin(),
				::tolower);
			current_tag = tags[tagname];

			switch (current_tag) {
			case HtmlTag::BLOCKQUOTE:
				--indent_level;
				if (indent_level < 0) {
					indent_level = 0;
				}
				add_nonempty_line(curline, tables, lines);
				add_line("", tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::OL:
				if (!ol_types.empty()) {
					ol_types.pop_back();
					ol_counts.pop_back();
				}
			// fall-through
			case HtmlTag::UL:
				if (list_elements_stack.size() >= 1
					&& list_elements_stack.back() == HtmlTag::LI) {
					list_elements_stack.pop_back();
					indent_level -= 2;
					if (indent_level < 0) {
						indent_level = 0;
					}
					add_nonempty_line(
						curline, tables, lines);
					prepare_new_line(curline,
						tables.size() ? 0
						: indent_level);
				}
				list_elements_stack.pop_back();
				add_nonempty_line(curline, tables, lines);
				add_line("", tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::DT:
				add_nonempty_line(curline, tables, lines);
				add_line("", tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::DD:
				indent_level -= 4;
				if (indent_level < 0) {
					indent_level = 0;
				}
				add_nonempty_line(curline, tables, lines);
				add_line("", tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::DL:
				// ignore tag
				break;

			case HtmlTag::LI:
				indent_level -= 2;
				if (indent_level < 0) {
					indent_level = 0;
				}
				if (list_elements_stack.size() >= 1
					&& list_elements_stack.back() == HtmlTag::LI) {
					list_elements_stack.pop_back();
				}
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::H1:
				if (line_is_nonempty(curline)) {
					add_line(curline, tables, lines);
					size_t llen =
						utils::strwidth_stfl(curline);
					prepare_new_line(curline,
						tables.size() ? 0
						: indent_level);
					add_line(std::string(llen, '-'),
						tables,
						lines);
				}
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::H2:
			case HtmlTag::H3:
			case HtmlTag::H4:
			case HtmlTag::H5:
			case HtmlTag::H6:
			case HtmlTag::P:
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::PRE:
				add_line_softwrappable(curline, lines);
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				inside_pre = false;
				break;

			case HtmlTag::SUB:
				curline.append("]");
				break;

			case HtmlTag::SUP:
				// has closing tag, but we render nothing.
				break;

			case HtmlTag::A:
				if (link_num != -1) {
					if (!raw_) {
						curline.append("</>");
					}
					curline.append(strprintf::fmt(
							"[%d]", link_num));
					link_num = -1;
				}
				break;

			case HtmlTag::UNDERLINE:
				if (!raw_) {
					curline.append("</>");
				}
				break;

			case HtmlTag::STRONG:
				if (!raw_) {
					curline.append("</>");
				}
				break;

			case HtmlTag::QUOTATION:
				if (!raw_) {
					curline.append("\"");
				}
				break;

			case HtmlTag::EMBED:
			case HtmlTag::BR:
			case HtmlTag::ITUNESHACK:
			case HtmlTag::IMG:
			case HtmlTag::HR:
			case HtmlTag::SOURCE:
				// ignore closing tags
				break;

			case HtmlTag::SCRIPT:
				// don't render scripts, ignore current line
				if (inside_script) {
					inside_script--;
				}
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::STYLE:
				if (inside_style) {
					inside_style--;
				}
				break;

			case HtmlTag::TABLE:
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(
					curline, 0); // no indent in tables

				if (!tables.empty()) {
					std::vector<std::pair<LineType,
					    std::string>>
					    table_text;
					tables.back().complete_cell();
					tables.back().complete_row();
					render_table(tables.back(), table_text);
					tables.pop_back();

					// still a table on the outside?
					if (!tables.empty()) {
						for (size_t idx = 0;
							idx < table_text.size();
							++idx)
							// add rendered table to current cell
							tables.back().add_text(
								table_text[idx]
								.second);
					} else {
						for (size_t idx = 0;
							idx < table_text.size();
							++idx) {
							std::string s =
								table_text[idx]
								.second;
							while (s.length() > 0 &&
								s[0] == '\n') {
								s.erase(0, 1);
							}
							add_line_nonwrappable(
								s, lines);
						}
					}
				}
				prepare_new_line(curline,
					tables.size() ? 0 : indent_level);
				break;

			case HtmlTag::TR:
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(
					curline, 0); // no indent in tables

				if (!tables.empty()) {
					tables.back().complete_row();
				}
				break;

			case HtmlTag::TH:
				if (!tables.empty()) {
					curline.append("</>");
				}

				add_nonempty_line(curline, tables, lines);
				prepare_new_line(
					curline, 0); // no indent in tables

				if (!tables.empty()) {
					tables.back().complete_cell();
				}
				break;

			case HtmlTag::TD:
				add_nonempty_line(curline, tables, lines);
				prepare_new_line(
					curline, 0); // no indent in tables

				if (!tables.empty()) {
					tables.back().complete_cell();
				}
				break;

			case HtmlTag::VIDEO:
				if (inside_video && source_count == 0) {
					video_count--;
				}

				inside_video = false;
				source_count = 0;
				break;

			case HtmlTag::AUDIO:
				if (inside_audio && source_count == 0) {
					audio_count--;
				}

				inside_audio = false;
				source_count = 0;
				break;
			}
			break;

		case TagSoupPullParser::Event::TEXT: {

			auto text = xpp.get_text();
			if (!raw_) {
				text = utils::quote_for_stfl(text);
			}

			if (itunes_hack) {
				std::vector<std::string> paragraphs =
					utils::tokenize_nl(text);
				for (const auto& paragraph : paragraphs) {
					if (paragraph != "\n") {
						add_nonempty_line(
							curline, tables, lines);
						prepare_new_line(curline,
							tables.size()
							? 0
							: indent_level);
						curline.append(paragraph);
					}
				}
			} else if (inside_pre) {
				std::vector<std::string> paragraphs =
					utils::tokenize_nl(text);
				for (const auto& paragraph : paragraphs) {
					if (paragraph == "\n") {
						add_line_softwrappable(
							curline, lines);
						prepare_new_line(curline,
							tables.size()
							? 0
							: indent_level);
					} else {
						curline.append(paragraph);
					}
				}
			} else if (inside_script || inside_style || inside_video || inside_audio) {
				// skip scripts, CSS styles and fallback text for media elements
			} else {
				// strip leading whitespace
				bool had_whitespace = false;
				while (text.length() > 0 &&
					::isspace(text[0])) {
					text.erase(0, 1);
					had_whitespace = true;
				}
				if (line_is_nonempty(curline) &&
					had_whitespace) {
					curline.append(" ");
				}
				// strip newlines
				text = std::string(utils::replace_all(text, "\n", " "));
				curline.append(text);
			}
		}
		break;
		default:
			/* do nothing */
			break;
		}
	}

	// and the rest
	add_nonempty_line(curline, tables, lines);

	// force all tables to be closed and rendered
	while (!tables.empty()) {
		std::vector<std::pair<LineType, std::string>> table_text;
		render_table(tables.back(), table_text);
		tables.pop_back();
		for (size_t idx = 0; idx < table_text.size(); ++idx) {
			std::string s = table_text[idx].second;
			while (s.length() > 0 && s[0] == '\n') {
				s.erase(0, 1);
			}
			add_line_nonwrappable(s, lines);
		}
	}

	// add link list
	if (links.size() > 0) {
		add_line("", tables, lines);
		add_line(_("Links: "), tables, lines);
		for (unsigned int i = 0; i < links.size(); ++i) {
			auto link_text = strprintf::fmt("[%u]: %s (%s)",
					i + 1,
					links[i].first,
					type2str(links[i].second));
			add_line_softwrappable(link_text, lines);
		}
	}
}

void HtmlRenderer::add_media_link(std::string& curline,
	std::vector<LinkPair>& links, const std::string& url,
	const std::string& media_url, unsigned int media_count, LinkType type)
{
	if (media_url.empty()) {
		return;
	}

	std::string type_str = type2str(type);
	unsigned int link_num = add_link(links,
			utils::censor_url(utils::absolute_url(url, media_url)),
			type);

	curline.append(strprintf::fmt("[%s %u (%s #%u)]", _(type_str.c_str()),
			media_count, _("link"), link_num));
}

std::string HtmlRenderer::render_hr(const unsigned int width)
{
	std::string result = "\n ";
	result += std::string(width - 2, '-');
	result += " \n";

	return result;
}

std::string HtmlRenderer::type2str(LinkType type)
{
	switch (type) {
	case LinkType::HREF:
		return _("link");
	case LinkType::IMG:
		return _("image");
	case LinkType::EMBED:
		return _("embedded flash");
	case LinkType::VIDEO:
		return _("video");
	case LinkType::AUDIO:
		return _("audio");
	default:
		return _("unknown (bug)");
	}
}

void HtmlRenderer::add_nonempty_line(const std::string& curline,
	std::vector<Table>& tables,
	std::vector<std::pair<LineType, std::string>>& lines)
{
	if (line_is_nonempty(curline)) {
		add_line(curline, tables, lines);
	}
}

void HtmlRenderer::add_hr(std::vector<std::pair<LineType, std::string>>& lines)
{
	lines.push_back(std::make_pair(LineType::hr, std::string("")));
}

void HtmlRenderer::add_line(const std::string& curline,
	std::vector<Table>& tables,
	std::vector<std::pair<LineType, std::string>>& lines)
{
	if (tables.size()) {
		tables.back().add_text(curline);
	} else {
		lines.push_back(std::make_pair(LineType::wrappable, curline));
	}
}

void HtmlRenderer::add_line_softwrappable(const std::string& line,
	std::vector<std::pair<LineType, std::string>>& lines)
{
	lines.push_back(std::make_pair(LineType::softwrappable, line));
}

void HtmlRenderer::add_line_nonwrappable(const std::string& line,
	std::vector<std::pair<LineType, std::string>>& lines)
{
	lines.push_back(std::make_pair(LineType::nonwrappable, line));
}

void HtmlRenderer::prepare_new_line(std::string& line, int indent_level)
{
	line = "";
	line.append(indent_level * 2, ' ');
}

bool HtmlRenderer::line_is_nonempty(const std::string& line)
{
	for (std::string::size_type i = 0; i < line.length(); ++i) {
		if (!isblank(line[i]) && line[i] != '\n' && line[i] != '\r') {
			return true;
		}
	}
	return false;
}

void HtmlRenderer::TableRow::start_cell(size_t span)
{
	inside = true;
	if (span < 1) {
		span = 1;
	}
	cells.push_back(TableCell(span));
}

void HtmlRenderer::TableRow::add_text(const std::string& str)
{
	if (!inside) {
		start_cell(1);        // colspan 1
	}

	cells.back().text.push_back(str);
}

void HtmlRenderer::TableRow::complete_cell()
{
	inside = false;
}

void HtmlRenderer::Table::start_cell(size_t span)
{
	if (!inside) {
		start_row();
	}
	rows.back().start_cell(span);
}

void HtmlRenderer::Table::complete_cell()
{
	if (rows.size()) {
		rows.back().complete_cell();
	}
}

void HtmlRenderer::Table::start_row()
{
	if (rows.size() && rows.back().inside) {
		rows.back().complete_cell();
	}
	inside = true;
	rows.push_back(TableRow());
}

void HtmlRenderer::Table::add_text(const std::string& str)
{
	if (!inside) {
		start_row();
	}
	rows.back().add_text(str);
}

void HtmlRenderer::Table::complete_row()
{
	inside = false;
}

void HtmlRenderer::render_table(const HtmlRenderer::Table& table,
	std::vector<std::pair<LineType, std::string>>& lines)
{
	// get number of rows
	size_t rows = table.rows.size();

	// get maximum number of cells
	size_t cells = 0;
	for (size_t row = 0; row < rows; row++) {
		size_t count = 0;
		for (size_t cell = 0; cell < table.rows[row].cells.size();
			cell++) {
			count += table.rows[row].cells[cell].span;
		}
		cells = std::max(cells, count);
	}

	// get width of each row
	std::vector<size_t> cell_widths;
	cell_widths.resize(cells, 0);
	for (size_t row = 0; row < rows; row++) {
		for (size_t cell = 0; cell < table.rows[row].cells.size();
			cell++) {
			size_t width = 0;
			if (table.rows[row].cells[cell].text.size()) {
				for (size_t idx = 0; idx <
					table.rows[row].cells[cell].text.size();
					idx++)
					width = std::max(width,
							utils::strwidth_stfl(
								table.rows[row]
								.cells[cell]
								.text[idx]));
			}
			if (table.rows[row].cells[cell].span > 1) {
				width += table.rows[row].cells[cell].span;
				// divide size evenly on columns (can be done better, I know)
				width /= table.rows[row]
					.cells[cell]
					.span;
			}
			cell_widths[cell] = std::max(cell_widths[cell], width);
		}
	}

	char hsep = '-';
	char vsep = '|';
	char hvsep = '+';

	// create a row separator
	std::string separator;
	if (table.has_border) {
		separator += hvsep;
	}
	for (size_t cell = 0; cell < cells; cell++) {
		separator += std::string(cell_widths[cell], hsep);
		separator += hvsep;
	}

	if (!table.has_border) {
		vsep = ' ';
	}

	// render the table
	if (table.has_border)
		lines.push_back(
			std::make_pair(LineType::nonwrappable, separator));
	for (size_t row = 0; row < rows; row++) {
		// calc height of this row
		size_t height = 0;
		for (size_t cell = 0; cell < table.rows[row].cells.size();
			cell++)
			height = std::max(height,
					table.rows[row].cells[cell].text.size());

		for (size_t idx = 0; idx < height; ++idx) {
			std::string line;
			if (table.has_border) {
				line += vsep;
			}
			for (size_t cell = 0;
				cell < table.rows[row].cells.size();
				cell++) {
				size_t cell_width = 0;
				if (idx < table.rows[row]
					.cells[cell]
					.text.size()) {
					LOG(Level::DEBUG,
						"row = %" PRIu64 " cell = %" PRIu64 " text = %s",
						static_cast<uint64_t>(row),
						static_cast<uint64_t>(cell),
						table.rows[row]
						.cells[cell]
						.text[idx]);
					cell_width = utils::strwidth_stfl(
							table.rows[row]
							.cells[cell]
							.text[idx]);
					line += table.rows[row]
						.cells[cell]
						.text[idx];
				}
				size_t reference_width = cell_widths[cell];
				if (table.rows[row].cells[cell].span > 1) {
					for (size_t ic = cell + 1; ic < cell +
						table.rows[row]
						.cells[cell]
						.span;
						++ic)
						reference_width +=
							cell_widths[ic] + 1;
				}
				LOG(Level::DEBUG,
					"cell_width = %" PRIu64 " reference_width = %" PRIu64,
					static_cast<uint64_t>(cell_width),
					static_cast<uint64_t>(reference_width));
				if (cell_width <
					reference_width) // pad, if necessary
					line += std::string(
							reference_width - cell_width,
							' ');

				if (cell < table.rows[row].cells.size() - 1) {
					line += vsep;
				}
			}
			if (table.has_border) {
				line += vsep;
			}
			lines.push_back(
				std::make_pair(LineType::nonwrappable, line));
		}
		if (table.has_border)
			lines.push_back(std::make_pair(
					LineType::nonwrappable, separator));
	}
}

std::string HtmlRenderer::get_char_numbering(unsigned int count)
{
	std::string result;
	do {
		count--;
		result.push_back('a' + (count % 26));
		count /= 26;
	} while (count > 0);
	std::reverse(result.begin(), result.end());
	return result;
}

std::string HtmlRenderer::get_roman_numbering(unsigned int count)
{
	unsigned int values[] = {
		1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1
	};
	const char* numerals[] = {"m",
			"cm",
			"d",
			"cd",
			"c",
			"xc",
			"l",
			"xl",
			"x",
			"ix",
			"v",
			"iv",
			"i"
		};
	std::string result;
	for (unsigned int i = 0; i < (sizeof(values) / sizeof(values[0]));
		i++) {
		while (count >= values[i]) {
			count -= values[i];
			result.append(numerals[i]);
		}
	}
	return result;
}

std::string HtmlRenderer::format_ol_count(unsigned int count, char type)
{
	switch (type) {
	case 'a':
		return get_char_numbering(count);
	case 'A': {
		std::string num = get_char_numbering(count);
		std::transform(num.begin(), num.end(), num.begin(), ::toupper);
		return num;
	}
	case 'i':
		return get_roman_numbering(count);
	case 'I': {
		std::string roman = get_roman_numbering(count);
		std::transform(
			roman.begin(), roman.end(), roman.begin(), ::toupper);
		return roman;
	}
	case '1':
	default:
		return strprintf::fmt("%2u", count);
	}
}

} // namespace newsboat
