//Log.cpp

#include "../include/Log.hpp"

Log::Log::Log(TimeDetail::TimePeriod && period, ChannelName && channel_name, std::string && data, std::vector<std::string_view>&& names, std::vector<LineView>&& lines) :
	m_valid(true),
	m_period(std::move(period)),
	m_channel_name(std::move(channel_name)),
	m_data(std::move(data)),
	m_names(std::move(names)),
	m_lines(std::move(lines))
{
}

Log::Log::Log(TimeDetail::TimePeriod && period, ChannelName && channel_name, std::string && data, ParserFunc parser) :
	m_period(std::move(period)),
	m_channel_name(std::move(channel_name)),
	m_data(std::move(data))
{
	m_valid = parser(m_data, m_names, m_lines);
}

bool Log::Log::isValid() const
{
	return m_valid;
}

TimeDetail::TimePeriod Log::Log::getPeriod() const
{
	return m_period;
}

const std::string & Log::Log::getChannelName() const
{
	return m_channel_name;
}

const std::string & Log::Log::getData() const
{
	return m_data;
}

const std::vector<std::string_view>& Log::Log::getNames() const
{
	return m_names;
}

const std::vector<Log::LineView> & Log::Log::getLines() const
{
	return m_lines;
}

void Log::Log::moveImpl(Log && source)
{
	m_valid = std::move(source.m_valid);
	m_channel_name = std::move(source.m_channel_name);
	m_period = std::move(source.m_period);
	m_data = std::move(source.m_data);
	m_lines = std::move(source.m_lines);
	if (source.m_data.size() <= sizeof(decltype(source.m_data))) {
		//handle SSO
		for (auto & line : m_lines) {
			line = LineView::copyView(source.m_data.data(), m_data.data(), line);
		}
	}
}

Log::Line::Line(const LineView & line_view)
{
	copyLineViewImpl(line_view);
}

Log::Line::Line(const Line & source)
{
	copyLineImpl(source);
}

Log::Line::Line(Line && source)
{
	moveLineImpl(std::move(source));
}

std::string & Log::Line::getData()
{
	return m_data;
}

TimeDetail::TimePoint Log::Line::getTime() const
{
	return m_time;
}

std::string_view Log::Line::getNameView() const
{
	return m_name_view;
}

std::string_view Log::Line::getMessageView() const
{
	return m_message_view;
}

void Log::Line::copyLineViewImpl(const LineView & line_view)
{
	m_time = line_view.getTime();
	m_data = line_view.getNameView();
	m_data.append(line_view.getMessageView());
	m_name_view = std::string_view(m_data.data(), line_view.getNameView().size());
	m_message_view = std::string_view(m_data.data() + m_name_view.size(), line_view.getMessageView().size());
}

void Log::Line::copyLineImpl(const Line & source)
{
	m_time = source.m_time;
	m_data = source.m_data;
	auto copy_view = [&](std::string_view source_view) {
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		return std::string_view(m_data.data() + distance, source_view.size());
	};
	m_name_view = copy_view(source.m_name_view);
	m_message_view = copy_view(source.m_message_view);
}

void Log::Line::moveLineImpl(Line && source)
{
	m_time = std::move(source.m_time);
	m_data = std::move(m_data);
	auto copy_view = [&](std::string_view source_view) {
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		return std::string_view(m_data.data() + distance, source_view.size());
	};
	m_name_view = copy_view(source.m_name_view);
	m_message_view = copy_view(source.m_message_view);
}

Log::LineView::LineView(TimeDetail::TimePoint time, std::string_view name_view, std::string_view message_view) :
	m_time(time),
	m_name_view(name_view),
	m_message_view(message_view)
{
}

TimeDetail::TimePoint Log::LineView::getTime() const
{
	return m_time;
}

std::string_view Log::LineView::getNameView() const
{
	return m_name_view;
}

std::string_view Log::LineView::getMessageView() const
{
	return m_message_view;
}

Log::LineView Log::LineView::copyView(const char * old_offset, const char * new_offset, LineView source)
{
	auto copy_view = [&](std::string_view source_view) {
		assert(source_view.data() >= old_offset);
		std::ptrdiff_t distance = source_view.data() - old_offset;
		return std::string_view(new_offset + distance, source_view.size());
	};
	LineView new_view(
		source.m_time,
		copy_view(source.m_name_view),
		copy_view(source.m_message_view)
	);
	return new_view;
}
