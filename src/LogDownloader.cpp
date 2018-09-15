//LogDownloader.cpp

#include "../include/LogDownloader.hpp"

Log::Log(std::string && source, std::string && data) :
	m_source(std::move(source)),
	m_data(std::move(data))
{
	parse();
}

const std::string & Log::getSource() const
{
	return m_source;
}

const std::string & Log::getData() const
{
	return m_data;
}

const Log::LineContainer & Log::getLines() const
{
	return m_lines;
}

void Log::parse()
{
	std::string_view data_view(m_data);
	while (!data_view.empty()) {
		
		auto cr_pos = data_view.find("\r\n");
		if (cr_pos == data_view.npos) {
			break;
		}

		std::string_view all(data_view.substr(0, cr_pos));

		auto prefix_end = all.find(": ");
		if (prefix_end == all.npos && prefix_end < all.size() - 2) {
			break;
		}
		std::string_view prefix(all.substr(0, prefix_end));
		std::string_view message(all.substr(prefix_end + 2));
		
		m_lines.emplace_back(all, prefix, message);

		//advance
		data_view.remove_prefix(cr_pos + 2);
	}
}
