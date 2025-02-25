#include <Common/PODArray.h>
#include <IO/WriteBuffer.h>
#include <IO/WriteHelpers.h>
#include <IO/Operators.h>
#include <Formats/FormatFactory.h>
#include <Processors/Formats/Impl/PrettyCompactBlockOutputFormat.h>


namespace DB
{

namespace ErrorCodes
{


}


namespace
{

/// Grid symbols are used for printing grid borders in a terminal.
/// Defaults values are UTF-8.
struct GridSymbols
{
    const char * left_top_corner = "┌";
    const char * right_top_corner = "┐";
    const char * left_bottom_corner = "└";
    const char * right_bottom_corner = "┘";
    const char * top_separator = "┬";
    const char * bottom_separator = "┴";
    const char * dash = "─";
    const char * bar = "│";
};

GridSymbols utf8_grid_symbols;

GridSymbols ascii_grid_symbols {
    "+",
    "+",
    "+",
    "+",
    "+",
    "+",
    "-",
    "|"
};

}

PrettyCompactBlockOutputFormat::PrettyCompactBlockOutputFormat(WriteBuffer & out_, const Block & header, const FormatSettings & format_settings_, bool mono_block_)
    : PrettyBlockOutputFormat(out_, header, format_settings_, mono_block_)
{
}

void PrettyCompactBlockOutputFormat::writeHeader(
    const Block & block,
    const Widths & max_widths,
    const Widths & name_widths)
{
    if (format_settings.pretty.output_format_pretty_row_numbers)
    {
        /// Write left blank
        writeString(String(row_number_width, ' '), out);
    }

    const GridSymbols & grid_symbols = format_settings.pretty.charset == FormatSettings::Pretty::Charset::UTF8 ?
                                       utf8_grid_symbols :
                                       ascii_grid_symbols;

    /// Names
    writeCString(grid_symbols.left_top_corner, out);
    writeCString(grid_symbols.dash, out);
    for (size_t i = 0; i < max_widths.size(); ++i)
    {
        if (i != 0)
        {
            writeCString(grid_symbols.dash, out);
            writeCString(grid_symbols.top_separator, out);
            writeCString(grid_symbols.dash, out);
        }

        const ColumnWithTypeAndName & col = block.getByPosition(i);

        if (col.type->shouldAlignRightInPrettyFormats())
        {
            for (size_t k = 0; k < max_widths[i] - name_widths[i]; ++k)
                writeCString(grid_symbols.dash, out);

            if (format_settings.pretty.color)
                writeCString("\033[1m", out);
            writeString(col.name, out);
            if (format_settings.pretty.color)
                writeCString("\033[0m", out);
        }
        else
        {
            if (format_settings.pretty.color)
                writeCString("\033[1m", out);
            writeString(col.name, out);
            if (format_settings.pretty.color)
                writeCString("\033[0m", out);

            for (size_t k = 0; k < max_widths[i] - name_widths[i]; ++k)
                writeCString(grid_symbols.dash, out);
        }
    }
    writeCString(grid_symbols.dash, out);
    writeCString(grid_symbols.right_top_corner, out);
    writeCString("\n", out);
}

void PrettyCompactBlockOutputFormat::writeBottom(const Widths & max_widths)
{
    if (format_settings.pretty.output_format_pretty_row_numbers)
    {
        /// Write left blank
        writeString(String(row_number_width, ' '), out);
    }

    const GridSymbols & grid_symbols = format_settings.pretty.charset == FormatSettings::Pretty::Charset::UTF8 ?
                                       utf8_grid_symbols :
                                       ascii_grid_symbols;
    /// Write delimiters
    out << grid_symbols.left_bottom_corner;
    for (size_t i = 0; i < max_widths.size(); ++i)
    {
        if (i != 0)
            out << grid_symbols.bottom_separator;

        for (size_t j = 0; j < max_widths[i] + 2; ++j)
            out << grid_symbols.dash;
    }
    out << grid_symbols.right_bottom_corner << "\n";
}

void PrettyCompactBlockOutputFormat::writeRow(
    size_t row_num,
    const Block & header,
    const Columns & columns,
    const WidthsPerColumn & widths,
    const Widths & max_widths)
{
    if (format_settings.pretty.output_format_pretty_row_numbers)
    {
        // Write row number;
        auto row_num_string = std::to_string(row_num + 1 + total_rows) + ". ";
        for (size_t i = 0; i < row_number_width - row_num_string.size(); ++i)
        {
            writeCString(" ", out);
        }
        writeString(row_num_string, out);
    }

    const GridSymbols & grid_symbols = format_settings.pretty.charset == FormatSettings::Pretty::Charset::UTF8 ?
                                       utf8_grid_symbols :
                                       ascii_grid_symbols;

    size_t num_columns = max_widths.size();

    writeCString(grid_symbols.bar, out);

    for (size_t j = 0; j < num_columns; ++j)
    {
        if (j != 0)
            writeCString(grid_symbols.bar, out);

        const auto & type = *header.getByPosition(j).type;
        const auto & cur_widths = widths[j].empty() ? max_widths[j] : widths[j][row_num];
        writeValueWithPadding(*columns[j], *serializations[j], row_num, cur_widths, max_widths[j], type.shouldAlignRightInPrettyFormats());
    }

    writeCString(grid_symbols.bar, out);
    writeCString("\n", out);
}

void PrettyCompactBlockOutputFormat::writeChunk(const Chunk & chunk, PortKind port_kind)
{
    UInt64 max_rows = format_settings.pretty.max_rows;

    size_t num_rows = chunk.getNumRows();
    const auto & header = getPort(port_kind).getHeader();
    const auto & columns = chunk.getColumns();

    WidthsPerColumn widths;
    Widths max_widths;
    Widths name_widths;
    calculateWidths(header, chunk, widths, max_widths, name_widths);

    writeHeader(header, max_widths, name_widths);

    for (size_t i = 0; i < num_rows && total_rows + i < max_rows; ++i)
        writeRow(i, header, columns, widths, max_widths);

    writeBottom(max_widths);

    if (port_kind != PortKind::PartialResult)
        total_rows += num_rows;
}


void registerOutputFormatPrettyCompact(FormatFactory & factory)
{
    registerPrettyFormatWithNoEscapesAndMonoBlock<PrettyCompactBlockOutputFormat>(factory, "PrettyCompact");
}

}
