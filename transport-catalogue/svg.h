#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace svg {

struct Rgb {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    Rgb() = default;

    Rgb(uint8_t r, uint8_t g, uint8_t b)
        : red(r), green(g), blue(b)
    {}
};

struct Rgba : Rgb {
    double opacity = 1.0;

    Rgba() = default;

    Rgba(uint8_t r, uint8_t g, uint8_t b, double o)
        : Rgb(r, g, b), opacity(o)
    {}
};

using Color = std::variant<std::monostate, std::string, Rgb, Rgba>;
inline const Color NoneColor{}; // std::monostate

struct ColorVisitor {
    std::ostream& out;

    void operator()(const std::monostate&) const {
        out << "none";
    }

    void operator()(const std::string& color) const {
        out << color;
    }

    void operator()(const svg::Rgb& rgb) const {
        out << "rgb(" 
            << static_cast<int>(rgb.red) << "," 
            << static_cast<int>(rgb.green) << "," 
            << static_cast<int>(rgb.blue) << ")";
    }

    void operator()(const svg::Rgba& rgba) const {
        out << "rgba(" 
            << static_cast<int>(rgba.red) << "," 
            << static_cast<int>(rgba.green) << "," 
            << static_cast<int>(rgba.blue) << "," 
            << rgba.opacity << ")";
    }
};

std::ostream& operator<<(std::ostream& out, const svg::Color& color);

enum class StrokeLineCap {
    BUTT,
    ROUND,
    SQUARE,
};

enum class StrokeLineJoin {
    ARCS,
    BEVEL,
    MITER,
    MITER_CLIP,
    ROUND,
};

// Перегрузка оператора << для StrokeLineCap
std::ostream& operator<<(std::ostream& out, StrokeLineCap line_cap);

// Перегрузка оператора << для StrokeLineJoin
std::ostream& operator<<(std::ostream& out, StrokeLineJoin line_join);


// Базовый класс для общих атрибутов path
template <typename Owner>
class PathProps {
public:
    Owner& SetFillColor(Color color) {
        fill_color_ = std::move(color);
        return AsOwner();
    }

    Owner& SetStrokeColor(Color color) {
        stroke_color_ = std::move(color);
        return AsOwner();
    }

    Owner& SetStrokeWidth(double width) {
        stroke_width_ = std::move(width);
        return AsOwner();
    }

    Owner& SetStrokeLineCap(StrokeLineCap line_cap) {
        stroke_line_cap_ = std::move(line_cap);
        return AsOwner();
    }

    Owner& SetStrokeLineJoin(StrokeLineJoin line_join) {
        stroke_line_join_ = std::move(line_join);
        return AsOwner();
    }

protected:
    ~PathProps() = default;
    void RenderAttrs(std::ostream& out) const {
        using namespace std::literals;
        bool first = true;
        if (fill_color_) {
            out << (first ? "" : " ") << "fill=\""sv << fill_color_.value() << "\" ";
            first = false;
        }
        if (stroke_color_) {
            out << "stroke=\""sv << stroke_color_.value() << "\" ";
            first = false;
        }
        if (stroke_width_) {
            out << "stroke-width=\""sv << stroke_width_.value() << "\" ";
            first = false;
        }
        if (stroke_line_cap_) {
            out << "stroke-linecap=\""sv << stroke_line_cap_.value() << "\" ";
            first = false;
        }
        if (stroke_line_join_) {
            out << "stroke-linejoin=\""sv << stroke_line_join_.value() << "\" ";
            first = false;
        }
    }

    std::optional<Color> fill_color_;
    std::optional<Color> stroke_color_;
    std::optional<double> stroke_width_;
    std::optional<StrokeLineCap> stroke_line_cap_;
    std::optional<StrokeLineJoin> stroke_line_join_;

private:
    // static_cast безопасно преобразует *this к Owner&,
    // если класс Owner — наследник PathProps
    Owner& AsOwner() {
        return static_cast<Owner&>(*this);
    }
};

struct Point {
    Point() = default;

    Point(double x, double y) : x(x), y(y) {}

    double x = 0;
    double y = 0;
};

/*
 * Вспомогательная структура, хранящая контекст для вывода SVG-документа с отступами.
 * Хранит ссылку на поток вывода, текущее значение и шаг отступа при выводе элемента
 */
struct RenderContext {
    RenderContext(std::ostream& out) : out(out) {}

    RenderContext(std::ostream& out, int indent_step, int indent = 0)
        : out(out), indent_step(indent_step), indent(indent)
    {}

    RenderContext Indented() const {
        return {out, indent_step, indent + indent_step};
    }

    void RenderIndent() const {
        for (int i = 0; i < indent; ++i) {
            out.put(' ');
        }
    }

    std::ostream& out;
    int indent_step = 0;
    int indent = 0;
};

/*
 * Абстрактный базовый класс Object служит для унифицированного хранения
 * конкретных тегов SVG-документа
 * Реализует паттерн "Шаблонный метод" для вывода содержимого тега
 */
class Object {
public:
    void Render(const RenderContext& context) const;

    virtual ~Object() = default;

private:
    virtual void RenderObject(const RenderContext& context) const = 0;
};

// Интерфейс ObjectContainer
class ObjectContainer {
public:
    virtual void AddPtr(std::unique_ptr<Object>&& obj) = 0;

    template <typename T>
    void Add(T obj) {
        AddPtr(std::make_unique<T>(std::move(obj)));
    }

protected:
    ~ObjectContainer() = default; // Защищённый невиртуальный деструктор
};

// Интерфейс Drawable
class Drawable {
public:
    virtual void Draw(ObjectContainer& container) const = 0;
    virtual ~Drawable() = default; // Виртуальный деструктор для полиморфного удаления
};

/*
 * Класс Circle моделирует элемент <circle> для отображения круга
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/circle
 */
class Circle final : public Object, public PathProps<Circle> {
public:
    Circle& SetCenter(Point center);
    Circle& SetRadius(double radius);

private:
    void RenderObject(const RenderContext& context) const override;

    Point center_;
    double radius_{1.0};
};

/*
 * Класс Polyline моделирует элемент <polyline> для отображения ломаных линий
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/polyline
 */
class Polyline final : public Object, public PathProps<Polyline> {
public:
    Polyline& AddPoint(Point point);

private:
    void RenderObject(const RenderContext& context) const override;

    std::vector<Point> points_;
};

/*
 * Класс Text моделирует элемент <text> для отображения текста
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/text
 */
class Text final : public Object, public PathProps<Text> {
public:
    Text& SetPosition(Point pos);
    Text& SetOffset(Point offset);
    Text& SetFontSize(uint32_t size);
    Text& SetFontFamily(std::string font_family);
    Text& SetFontWeight(std::string font_weight);
    Text& SetData(std::string data);

private:
    void RenderObject(const RenderContext& context) const override;

    Point pos_{0.0, 0.0};
    Point offset_{0.0, 0.0};
    uint32_t font_size_{1};
    std::string font_family_;
    std::string font_weight_;
    std::string data_;
};

// Класс Document
class Document : public ObjectContainer {
public:
    void AddPtr(std::unique_ptr<Object>&& obj) override;
    void Render(std::ostream& out) const;

private:
    std::vector<std::unique_ptr<Object>> objects_;
};

}  // namespace svg