#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

using namespace std;

enum CommandType : uint8_t
{
    CLEAR_DISPLAY = 0x01,
    DRAW_PIXEL = 0x02,
    DRAW_LINE = 0x03,
    DRAW_RECTANGLE = 0x04,
    FILL_RECTANGLE = 0x05,
    DRAW_ELLIPSE = 0x06,
    FILL_ELLIPSE = 0x07,
    DRAW_CIRCLE = 0x08,
    FILL_CIRCLE = 0x09,
    DRAW_ROUNDED_RECTANGLE = 0x0A,
    FILL_ROUNDED_RECTANGLE = 0x0B,
    DRAW_TEXT = 0x0C,
    DRAW_IMAGE = 0x0D,
    SET_ORIENTATION = 0x0E,
    GET_WIDTH = 0x0F,
    GET_HEIGHT = 0x10
};

struct Command
{
    CommandType type;
    vector<int16_t> parameters;
    uint16_t color = 0;
    uint8_t font = 0;
    uint8_t orientation = 0;
    string text;
    vector<uint8_t> imageData;
};

class DisplayProtocol
{
private:
    const vector<uint8_t>& data;
    size_t pos = 0;

    uint8_t readByte()
    {
        if (pos >= data.size())
            throw runtime_error("Not enough data");

        return data[pos++];
    }

    uint16_t readUInt16()
    {
        uint16_t low = readByte();
        uint16_t high = readByte();

        return low | (high << 8);
    }

    int16_t readInt16()
    {
        return static_cast<int16_t>(readUInt16());
    }

    void checkPositive(int16_t value)
    {
        if (value <= 0)
            throw runtime_error("Value must be positive");
    }

    void checkNonNegative(int16_t value)
    {
        if (value < 0)
            throw runtime_error("Value cannot be negative");
    }

public:
    DisplayProtocol(const vector<uint8_t>& bytes)
        : data(bytes)
    {
    }

    vector<Command> parse()
    {
        vector<Command> commands;

        while (pos < data.size())
        {
            commands.push_back(parseCommand());
        }

        return commands;
    }

    Command parseCommand()
    {
        Command cmd;
        cmd.type = static_cast<CommandType>(readByte());

        switch (cmd.type)
        {
        case CLEAR_DISPLAY:
            cmd.color = readUInt16();
            break;

        case DRAW_PIXEL:
        {
            int16_t x = readInt16();
            int16_t y = readInt16();

            cmd.parameters = { x, y };
            cmd.color = readUInt16();
            break;
        }

        case DRAW_LINE:
        {
            int16_t x0 = readInt16();
            int16_t y0 = readInt16();
            int16_t x1 = readInt16();
            int16_t y1 = readInt16();

            cmd.parameters = { x0, y0, x1, y1 };
            cmd.color = readUInt16();
            break;
        }

        case DRAW_RECTANGLE:
        case FILL_RECTANGLE:
        {
            int16_t x = readInt16();
            int16_t y = readInt16();
            int16_t w = readInt16();
            int16_t h = readInt16();

            checkPositive(w);
            checkPositive(h);

            cmd.parameters = { x, y, w, h };
            cmd.color = readUInt16();
            break;
        }

        case DRAW_ELLIPSE:
        case FILL_ELLIPSE:
        {
            int16_t x = readInt16();
            int16_t y = readInt16();
            int16_t rx = readInt16();
            int16_t ry = readInt16();

            checkNonNegative(rx);
            checkNonNegative(ry);

            cmd.parameters = { x, y, rx, ry };
            cmd.color = readUInt16();
            break;
        }

        case DRAW_CIRCLE:
        case FILL_CIRCLE:
        {
            int16_t x = readInt16();
            int16_t y = readInt16();
            int16_t radius = readInt16();

            checkNonNegative(radius);

            cmd.parameters = { x, y, radius };
            cmd.color = readUInt16();
            break;
        }

        case DRAW_ROUNDED_RECTANGLE:
        case FILL_ROUNDED_RECTANGLE:
        {
            int16_t x = readInt16();
            int16_t y = readInt16();
            int16_t w = readInt16();
            int16_t h = readInt16();
            int16_t radius = readInt16();

            checkPositive(w);
            checkPositive(h);
            checkNonNegative(radius);

            if (radius > w / 2 || radius > h / 2)
                throw runtime_error("Invalid corner radius");

            cmd.parameters = { x, y, w, h, radius };
            cmd.color = readUInt16();
            break;
        }

        case DRAW_TEXT:
        {
            int16_t x = readInt16();
            int16_t y = readInt16();

            cmd.parameters = { x, y };
            cmd.color = readUInt16();
            cmd.font = readByte();

            uint16_t length = readUInt16();
            cout << "Text length: " << length << endl;
            cout << "Remaining bytes: " << data.size() - pos << endl;

            if (length > data.size() - pos)
                throw runtime_error("Invalid text length");

            for (uint16_t i = 0; i < length; i++)
                cmd.text += static_cast<char>(readByte());

            break;
        }

        case DRAW_IMAGE:
        {
            int16_t x = readInt16();
            int16_t y = readInt16();
            int16_t w = readInt16();
            int16_t h = readInt16();

            checkPositive(w);
            checkPositive(h);

            size_t imageSize = static_cast<size_t>(w) * h * 2;

            if (imageSize > data.size() - pos)
                throw runtime_error("Invalid image data");

            cmd.parameters = { x, y, w, h };

            for (size_t i = 0; i < imageSize; i++)
                cmd.imageData.push_back(readByte());

            break;
        }

        case SET_ORIENTATION:
        {
            cmd.orientation = readByte();

            if (cmd.orientation > 3)
                throw runtime_error("Invalid orientation");

            break;
        }

        case GET_WIDTH:
        case GET_HEIGHT:
            break;

        default:
            throw runtime_error("Unknown command");
        }

        return cmd;
    }
};