#include <Parsers/DumpASTNode.h>

#include <Common/FieldVisitorToString.h>
#include <Parsers/ASTFunction.h>
#include <Parsers/ASTIdentifier.h>
#include <Parsers/ASTLiteral.h>
#include <Parsers/ASTOrderByElement.h>
#include <Parsers/ASTSelectIntersectExceptQuery.h>
#include <Parsers/ASTSelectQuery.h>
#include <Parsers/ASTSelectWithUnionQuery.h>
#include <Parsers/NullsAction.h>
#include <Parsers/SelectUnionMode.h>


namespace DB
{

namespace
{

const char * functionKindToString(ASTFunction::Kind kind)
{
    switch (kind)
    {
        case ASTFunction::Kind::ORDINARY_FUNCTION: return "ORDINARY_FUNCTION";
        case ASTFunction::Kind::WINDOW_FUNCTION:   return "WINDOW_FUNCTION";
        case ASTFunction::Kind::LAMBDA_FUNCTION:   return "LAMBDA_FUNCTION";
        case ASTFunction::Kind::TABLE_ENGINE:      return "TABLE_ENGINE";
        case ASTFunction::Kind::DATABASE_ENGINE:   return "DATABASE_ENGINE";
        case ASTFunction::Kind::BACKUP_NAME:       return "BACKUP_NAME";
        case ASTFunction::Kind::CODEC:             return "CODEC";
        case ASTFunction::Kind::STATISTICS:        return "STATISTICS";
    }
    return "";
}

const char * nullsActionToString(NullsAction action)
{
    switch (action)
    {
        case NullsAction::EMPTY:         return "EMPTY";
        case NullsAction::RESPECT_NULLS: return "RESPECT NULLS";
        case NullsAction::IGNORE_NULLS:  return "IGNORE NULLS";
    }
    return "";
}

const char * fieldTypeName(Field::Types::Which which)
{
    switch (which)
    {
        case Field::Types::Null:                   return "Null";
        case Field::Types::UInt64:                 return "UInt64";
        case Field::Types::Int64:                  return "Int64";
        case Field::Types::Float64:                return "Float64";
        case Field::Types::UInt128:                return "UInt128";
        case Field::Types::Int128:                 return "Int128";
        case Field::Types::UInt256:                return "UInt256";
        case Field::Types::Int256:                 return "Int256";
        case Field::Types::String:                 return "String";
        case Field::Types::Array:                  return "Array";
        case Field::Types::Tuple:                  return "Tuple";
        case Field::Types::Map:                    return "Map";
        case Field::Types::Object:                 return "Object";
        case Field::Types::Bool:                   return "Bool";
        case Field::Types::UUID:                   return "UUID";
        case Field::Types::IPv4:                   return "IPv4";
        case Field::Types::IPv6:                   return "IPv6";
        case Field::Types::Decimal32:              return "Decimal32";
        case Field::Types::Decimal64:              return "Decimal64";
        case Field::Types::Decimal128:             return "Decimal128";
        case Field::Types::Decimal256:             return "Decimal256";
        case Field::Types::AggregateFunctionState: return "AggregateFunctionState";
        case Field::Types::CustomType:             return "CustomType";
    }
    return "";
}

/// Map a Field to a JSONBuilder value.
/// Common scalar Field types are emitted as native JSON values; everything
/// else falls back to a string produced by `FieldVisitorToString`.
JSONBuilder::ItemPtr fieldToJSON(const Field & value)
{
    switch (value.getType())
    {
        case Field::Types::Null:
            return std::make_unique<JSONBuilder::JSONNull>();
        case Field::Types::Bool:
            /// Bool is stored as UInt64 under the hood.
            return std::make_unique<JSONBuilder::JSONBool>(value.safeGet<UInt64>() != 0);
        case Field::Types::UInt64:
            return std::make_unique<JSONBuilder::JSONNumber<UInt64>>(value.safeGet<UInt64>());
        case Field::Types::Int64:
            return std::make_unique<JSONBuilder::JSONNumber<Int64>>(value.safeGet<Int64>());
        case Field::Types::Float64:
            return std::make_unique<JSONBuilder::JSONNumber<Float64>>(value.safeGet<Float64>());
        case Field::Types::String:
            return std::make_unique<JSONBuilder::JSONString>(value.safeGet<String>());
        default:
            return std::make_unique<JSONBuilder::JSONString>(applyVisitor(FieldVisitorToString(), value));
    }
}

/// Emit the children of an `ASTExpressionList` as a JSON array, inlining the
/// wrapper node itself. The wrapper is a parser-internal detail, so consumers
/// see the clause's elements directly. A null `list` yields an empty array.
JSONBuilder::ItemPtr inlineExpressionList(const ASTPtr & list)
{
    auto array = std::make_unique<JSONBuilder::JSONArray>();
    if (list)
        for (const auto & child : list->children)
            array->add(formatASTAsJSON(*child));
    return array;
}

/// Emit a single sub-node under `key`, but only when it is present.
void addNodeSlot(JSONBuilder::JSONMap & node, const char * key, const ASTPtr & child)
{
    if (child)
        node.add(key, formatASTAsJSON(*child));
}

/// Add per-class structured fields to `node`. Returns `true` when the class
/// exposes all of its sub-nodes through named slots and the generic
/// positional `children` array must therefore be suppressed.
bool enrichNode(JSONBuilder::JSONMap & node, const IAST & ast)
{
    if (const auto * function = dynamic_cast<const ASTFunction *>(&ast))
    {
        node.add("name", function->name);

        if (function->isOperator())
            node.add("is_operator", true);
        if (function->isWindowFunction())
            node.add("is_window_function", true);
        if (function->isLambdaFunction())
            node.add("is_lambda_function", true);

        if (function->getKind() != ASTFunction::Kind::ORDINARY_FUNCTION)
            node.add("kind", String(functionKindToString(function->getKind())));

        if (function->getNullsAction() != NullsAction::EMPTY)
            node.add("nulls_action", String(nullsActionToString(function->getNullsAction())));

        /// `arguments` is always present (possibly empty), so consumers never
        /// have to branch on its presence.
        node.add("arguments", inlineExpressionList(function->arguments));

        /// `parameters` only exists for parametric aggregates, e.g. quantile(0.9)(x).
        if (function->parameters)
            node.add("parameters", inlineExpressionList(function->parameters));

        /// Exactly one of `window_definition` / `window_name` is present for
        /// window functions; both absent otherwise. `window_definition` is not
        /// an `ASTExpressionList`, so it is emitted as a node.
        if (function->window_definition)
            node.add("window_definition", formatASTAsJSON(*function->window_definition));
        if (!function->window_name.empty())
            node.add("window_name", function->window_name);

        return true;
    }
    else if (const auto * table_identifier = dynamic_cast<const ASTTableIdentifier *>(&ast))
    {
        node.add("name", table_identifier->shortName());

        const auto database = table_identifier->getDatabaseName();
        if (!database.empty())
            node.add("database", database);
    }
    else if (const auto * identifier = dynamic_cast<const ASTIdentifier *>(&ast))
    {
        node.add("name", identifier->name());

        if (identifier->compound())
        {
            auto parts = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & part : identifier->name_parts)
                parts->add(part);
            node.add("name_parts", std::move(parts));
        }
    }
    else if (const auto * literal = dynamic_cast<const ASTLiteral *>(&ast))
    {
        node.add("value_type", String(fieldTypeName(literal->value.getType())));
        node.add("value", fieldToJSON(literal->value));
    }
    else if (const auto * order_by = dynamic_cast<const ASTOrderByElement *>(&ast))
    {
        /// The sort expression is the mandatory first child; collation and the
        /// WITH FILL bounds are optional named slots.
        if (!order_by->children.empty())
            node.add("expression", formatASTAsJSON(*order_by->children.front()));

        node.add("direction", String(order_by->direction >= 0 ? "ASC" : "DESC"));
        if (order_by->nulls_direction_was_explicitly_specified)
            node.add("nulls_first", order_by->nulls_direction != order_by->direction);

        addNodeSlot(node, "collation", order_by->getCollation());

        if (order_by->with_fill)
            node.add("with_fill", true);
        addNodeSlot(node, "fill_from", order_by->getFillFrom());
        addNodeSlot(node, "fill_to", order_by->getFillTo());
        addNodeSlot(node, "fill_step", order_by->getFillStep());
        addNodeSlot(node, "fill_staleness", order_by->getFillStaleness());

        return true;
    }
    else if (const auto * intersect_except = dynamic_cast<const ASTSelectIntersectExceptQuery *>(&ast))
    {
        /// Derives from ASTSelectQuery but, unlike it, keeps its operand
        /// selects in the positional `children` array rather than in the
        /// `Expression` slots — so it must be matched before ASTSelectQuery and
        /// must NOT suppress `children`.
        if (intersect_except->final_operator != ASTSelectIntersectExceptQuery::Operator::UNKNOWN)
            node.add("operator", String(ASTSelectIntersectExceptQuery::fromOperator(intersect_except->final_operator)));
    }
    else if (const auto * select = dynamic_cast<const ASTSelectQuery *>(&ast))
    {
        if (select->distinct)
            node.add("distinct", true);
        if (select->group_by_all)
            node.add("group_by_all", true);
        if (select->group_by_with_totals)
            node.add("group_by_with_totals", true);
        if (select->group_by_with_rollup)
            node.add("group_by_with_rollup", true);
        if (select->group_by_with_cube)
            node.add("group_by_with_cube", true);
        if (select->group_by_with_grouping_sets)
            node.add("group_by_with_grouping_sets", true);
        if (select->order_by_all)
            node.add("order_by_all", true);
        if (select->recursive_with)
            node.add("recursive_with", true);

        /// Expose each clause through a named slot, in declaration order.
        /// List-shaped clauses inline their `ASTExpressionList` wrapper; the
        /// rest are emitted as single nodes. `ALIASES` / `CTE_ALIASES` are
        /// analyzer state and never appear on parsed-but-not-analyzed ASTs, so
        /// they are intentionally omitted.
        using Expression = ASTSelectQuery::Expression;
        struct Slot
        {
            Expression expr;
            const char * key;
            bool is_list;
        };
        static constexpr Slot slots[] = {
            {Expression::WITH,            "with",            true},
            {Expression::SELECT,          "select",          true},
            {Expression::TABLES,          "tables",          false},
            {Expression::PREWHERE,        "prewhere",        false},
            {Expression::WHERE,           "where",           false},
            {Expression::GROUP_BY,        "group_by",        true},
            {Expression::HAVING,          "having",          false},
            {Expression::WINDOW,          "window",          true},
            {Expression::QUALIFY,         "qualify",         false},
            {Expression::ORDER_BY,        "order_by",        true},
            {Expression::LIMIT_BY_OFFSET, "limit_by_offset", false},
            {Expression::LIMIT_BY_LENGTH, "limit_by_length", false},
            {Expression::LIMIT_BY,        "limit_by",        true},
            {Expression::LIMIT_OFFSET,    "limit_offset",    false},
            {Expression::LIMIT_LENGTH,    "limit_length",    false},
            {Expression::SETTINGS,        "settings",        false},
            {Expression::INTERPOLATE,     "interpolate",     true},
        };

        for (const auto & slot : slots)
        {
            auto expr = select->getExpression(slot.expr);
            if (!expr)
                continue;
            if (slot.is_list)
                node.add(slot.key, inlineExpressionList(expr));
            else
                node.add(slot.key, formatASTAsJSON(*expr));
        }

        return true;
    }
    else if (const auto * select_union = dynamic_cast<const ASTSelectWithUnionQuery *>(&ast))
    {
        if (select_union->hasNonDefaultUnionMode())
            node.add("union_mode", String(toString(select_union->union_mode)));
    }

    return false;
}

}

JSONBuilder::ItemPtr formatASTAsJSON(const IAST & ast)
{
    auto node = std::make_unique<JSONBuilder::JSONMap>();

    /// `IAST::getID` packs auxiliary data into the string (e.g. function name,
    /// literal value). Strip the suffix so that "type" stays a clean class id;
    /// the structured info is exposed separately via enrichNode.
    String full_id = ast.getID(' ');
    auto space_pos = full_id.find(' ');
    if (space_pos != String::npos)
        full_id.resize(space_pos);
    node->add("type", std::move(full_id));

    String alias = ast.tryGetAlias();
    if (!alias.empty())
        node->add("alias", alias);

    bool handled_children = enrichNode(*node, ast);

    if (!handled_children && !ast.children.empty())
    {
        auto children = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & child : ast.children)
            children->add(formatASTAsJSON(*child));
        node->add("children", std::move(children));
    }

    return node;
}

}
