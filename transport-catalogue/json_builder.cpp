#include "json_builder.h"
#include <exception>
#include <variant>
#include <utility>

using namespace std::literals;

namespace json {

// Конструктор инициализирует пустой корневой узел (nullptr) и помещает указатель на него в стек.
// Стек nodes_stack_ управляет текущим контекстом построения: изначально это корень.
// Все операции начинаются отсюда — например, StartDict() или Value() заменят корень.
Builder::Builder()
    : root_()
    , nodes_stack_{&root_}
{}

// Завершает построение JSON: проверяет, что все начатые контейнеры (массивы/словари) закрыты.
// Если стек не пуст — значит, где-то не вызвали EndDict/EndArray — ошибка.
// После Build() объект Builder считается "финализированным" и использовать его нельзя.
Node Builder::Build() {
    if (!nodes_stack_.empty()) {
        throw std::logic_error("Attempt to build JSON which isn't finalized"s);
    }
    return std::move(root_);
}

// Устанавливает ключ в текущем словаре. Должен вызываться только внутри Dict.
// Проверяет, что текущий узел — словарь (Dict), и добавляет в него новый ключ.
// Создаёт пустой узел (nullptr) по этому ключу и помещает его в стек — теперь можно присвоить значение.
// Возвращает DictValueContext — контекст, в котором разрешён только Value().
Builder::DictValueContext Builder::Key(std::string key) {
    Node::Value& host_value = GetCurrentValue();
    if (!std::holds_alternative<Dict>(host_value)) {
        throw std::logic_error("Key() outside a dict"s);
    }
    nodes_stack_.push_back(&std::get<Dict>(host_value)[std::move(key)]);
    return BaseContext{*this};
}

// Добавляет значение: может быть вызван в трёх контекстах:
// 1. Внутри массива — значение добавляется в конец.
// 2. После Key() — устанавливается значение по ключу.
// 3. В корне — заменяет корневое значение (если ещё не финализировано).
// one_shot = true — после добавления не нужно оставаться "внутри" нового значения.
Builder::BaseContext Builder::Value(Node::Value value) {
    AddObject(std::move(value), /* one_shot */ true);
    return *this;
}

// Начинает новый словарь. Добавляет пустой Dict в текущий узел (массив или словарь).
// Если текущий узел — массив, Dict добавляется как элемент.
// Если текущий узел — словарь, Dict становится значением по ключу.
// one_shot = false — мы "входим" внутрь нового словаря, и он остаётся в стеке.
Builder::DictItemContext Builder::StartDict() {
    AddObject(Dict{}, /* one_shot */ false);
    return BaseContext{*this};
}

// Начинает новый массив. Аналогично StartDict, но создаёт пустой Array.
// Новый массив добавляется в текущий контейнер (массив или словарь).
// one_shot = false — мы остаёмся внутри массива и можем добавлять элементы.
// Указатель на новый массив кладётся в стек для последующих операций.
Builder::ArrayItemContext Builder::StartArray() {
    AddObject(Array{}, /* one_shot */ false);
    return BaseContext{*this};
}

// Завершает текущий словарь. Проверяет, что вершина стека — это Dict.
// Убирает последний узел из стека — возвращаемся на уровень выше.
// Вызывается после последнего Key().Value(), чтобы "выйти" из словаря.
// После EndDict() можно продолжить добавлять пары Key().Value() или завершить родителя.
Builder::BaseContext Builder::EndDict() {
    if (!std::holds_alternative<Dict>(GetCurrentValue())) {
        throw std::logic_error("EndDict() outside a dict"s);
    }
    nodes_stack_.pop_back();
    return *this;
}

// Завершает текущий массив. Проверяет, что вершина стека — это Array.
// Убирает массив из стека — возвращаемся к родительскому контейнеру.
// Вызывается после последнего элемента массива.
// После EndArray() можно продолжить заполнение родителя.
Builder::BaseContext Builder::EndArray() {
    if (!std::holds_alternative<Array>(GetCurrentValue())) {
        throw std::logic_error("EndArray() outside an array"s);
    }
    nodes_stack_.pop_back();
    return *this;
}

// Возвращает ссылку на значение текущего узла — того, на который указывает вершина стека.
// Используется для проверки типа (например, holds_alternative) и модификации.
// Если стек пуст — JSON уже построен, дальнейшие операции запрещены.
// Это неконстантная версия — для изменения узла.
Node::Value& Builder::GetCurrentValue() {
    if (nodes_stack_.empty()) {
        throw std::logic_error("Attempt to change finalized JSON"s);
    }
    return nodes_stack_.back()->GetValue();
}

// Константная версия GetCurrentValue — нужна для методов, которые не изменяют Builder,
// например, AssertNewObjectContext. Поведение идентично неконстантной версии.
// Обеспечивает доступ к значению для проверки типа без возможности модификации.
// Также проверяет, что стек не пуст — нельзя работать с финализированным JSON.
const Node::Value& Builder::GetCurrentValue() const {
    if (nodes_stack_.empty()) {
        throw std::logic_error("Attempt to change finalized JSON"s);
    }
    return nodes_stack_.back()->GetValue();
}

// Проверяет, что текущий узел — пустой (std::nullptr_t), то есть не инициализирован.
// Это необходимо перед присвоением значения (например, при Value() или StartDict() в корне).
// Если узел уже содержит что-то — значит, попытка перезаписать, что запрещено.
// Исключение предотвращает неконсистентное состояние JSON.
void Builder::AssertNewObjectContext() const {
    if (!std::holds_alternative<std::nullptr_t>(GetCurrentValue())) {
        throw std::logic_error("New object in wrong context"s);
    }
}

// Универсальный метод добавления значения в текущий узел.
// Если текущий узел — массив: добавляет значение через emplace_back (эффективно).
// Если нет — значит, добавляем в пустой узел (nullptr), но только если он пуст (AssertNewObjectContext).
// one_shot = false — для StartDict/StartArray: остаёмся внутри нового контейнера (в стеке).
// one_shot = true — для Value(): добавили и вышли (если это не вложенный контейнер).
void Builder::AddObject(Node::Value value, bool one_shot) {
    Node::Value& host_value = GetCurrentValue();
    if (std::holds_alternative<Array>(host_value)) {
        Node& node = std::get<Array>(host_value).emplace_back(std::move(value));
        if (!one_shot) {
            nodes_stack_.push_back(&node);
        }
    } else {
        AssertNewObjectContext();
        host_value = std::move(value);
        if (one_shot) {
            nodes_stack_.pop_back();
        }
    }
}

}  // namespace json
