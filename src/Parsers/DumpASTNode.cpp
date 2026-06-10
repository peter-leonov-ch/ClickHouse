#include <Parsers/DumpASTNode.h>

#include <Common/FieldVisitorToString.h>
#include <Parsers/ASTAsterisk.h>
#include <Parsers/ASTAlterQuery.h>
#include <Parsers/ASTAssignment.h>
#include <Parsers/ASTCheckQuery.h>
#include <Parsers/ASTCreateIndexQuery.h>
#include <Parsers/ASTDropIndexQuery.h>
#include <Parsers/ASTExplainQuery.h>
#include <Parsers/ASTKillQueryQuery.h>
#include <Parsers/ASTNameTypePair.h>
#include <Parsers/ASTRenameQuery.h>
#include <Parsers/ASTShowTablesQuery.h>
#include <Parsers/ASTStatisticsDeclaration.h>
#include <Parsers/ASTSystemQuery.h>
#include <Parsers/ASTUseQuery.h>
#include <Parsers/TablePropertiesQueriesASTs.h>
#include <Parsers/ASTColumnDeclaration.h>
#include <Parsers/ASTColumnsMatcher.h>
#include <Parsers/ASTColumnsTransformers.h>
#include <Parsers/ASTConstraintDeclaration.h>
#include <Parsers/ASTCreateFunctionQuery.h>
#include <Parsers/ASTCreateQuery.h>
#include <Parsers/ASTDataType.h>
#include <Parsers/ASTDeleteQuery.h>
#include <Parsers/ASTDictionary.h>
#include <Parsers/ASTDictionaryAttributeDeclaration.h>
#include <Parsers/ASTFunctionWithKeyValueArguments.h>
#include <Parsers/ASTDropQuery.h>
#include <Parsers/ASTFunction.h>
#include <Parsers/ASTIndexDeclaration.h>
#include <Parsers/ASTInsertQuery.h>
#include <Parsers/ASTOptimizeQuery.h>
#include <Parsers/ASTPartition.h>
#include <Parsers/ASTProjectionDeclaration.h>
#include <Parsers/ASTProjectionSelectQuery.h>
#include <Parsers/ASTQueryWithTableAndOutput.h>
#include <Parsers/ASTTTLElement.h>
#include <Parsers/ASTUpdateQuery.h>
#include <Parsers/ASTViewTargets.h>
#include <Parsers/ASTIdentifier.h>
#include <Parsers/ASTInterpolateElement.h>
#include <Parsers/ASTLiteral.h>
#include <Parsers/ASTOrderByElement.h>
#include <Parsers/ASTQualifiedAsterisk.h>
#include <Parsers/ASTSampleRatio.h>
#include <Parsers/ASTSelectIntersectExceptQuery.h>
#include <Parsers/ASTSelectQuery.h>
#include <Parsers/ASTSelectWithUnionQuery.h>
#include <Parsers/ASTSetQuery.h>
#include <Parsers/ASTSubquery.h>
#include <Parsers/ASTTablesInSelectQuery.h>
#include <Parsers/ASTWindowDefinition.h>
#include <Parsers/ASTWithElement.h>
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

const char * windowFrameTypeToString(WindowFrame::FrameType type)
{
    switch (type)
    {
        case WindowFrame::FrameType::ROWS:   return "ROWS";
        case WindowFrame::FrameType::GROUPS: return "GROUPS";
        case WindowFrame::FrameType::RANGE:  return "RANGE";
    }
    return "";
}

const char * windowBoundaryTypeToString(WindowFrame::BoundaryType type)
{
    switch (type)
    {
        case WindowFrame::BoundaryType::Unbounded: return "Unbounded";
        case WindowFrame::BoundaryType::Current:   return "Current";
        case WindowFrame::BoundaryType::Offset:    return "Offset";
    }
    return "";
}

const char * ttlModeToString(TTLMode mode)
{
    switch (mode)
    {
        case TTLMode::DELETE:     return "DELETE";
        case TTLMode::MOVE:       return "MOVE";
        case TTLMode::GROUP_BY:   return "GROUP_BY";
        case TTLMode::RECOMPRESS: return "RECOMPRESS";
    }
    return "";
}

const char * dataDestinationTypeToString(DataDestinationType type)
{
    switch (type)
    {
        case DataDestinationType::DISK:   return "DISK";
        case DataDestinationType::VOLUME: return "VOLUME";
        case DataDestinationType::TABLE:  return "TABLE";
        case DataDestinationType::DELETE: return "DELETE";
        case DataDestinationType::SHARD:  return "SHARD";
    }
    return "";
}

const char * constraintTypeToString(ASTConstraintDeclaration::Type type)
{
    switch (type)
    {
        case ASTConstraintDeclaration::Type::CHECK:  return "CHECK";
        case ASTConstraintDeclaration::Type::ASSUME: return "ASSUME";
    }
    return "";
}

const char * alterObjectTypeToString(ASTAlterQuery::AlterObjectType type)
{
    switch (type)
    {
        case ASTAlterQuery::AlterObjectType::TABLE:    return "TABLE";
        case ASTAlterQuery::AlterObjectType::DATABASE: return "DATABASE";
        case ASTAlterQuery::AlterObjectType::UNKNOWN:  return "UNKNOWN";
    }
    return "";
}

const char * alterCommandTypeToString(ASTAlterCommand::Type type)
{
    switch (type)
    {
        case ASTAlterCommand::ADD_COLUMN:               return "ADD_COLUMN";
        case ASTAlterCommand::DROP_COLUMN:              return "DROP_COLUMN";
        case ASTAlterCommand::MODIFY_COLUMN:            return "MODIFY_COLUMN";
        case ASTAlterCommand::COMMENT_COLUMN:           return "COMMENT_COLUMN";
        case ASTAlterCommand::RENAME_COLUMN:            return "RENAME_COLUMN";
        case ASTAlterCommand::MATERIALIZE_COLUMN:       return "MATERIALIZE_COLUMN";
        case ASTAlterCommand::MODIFY_ORDER_BY:          return "MODIFY_ORDER_BY";
        case ASTAlterCommand::MODIFY_SAMPLE_BY:         return "MODIFY_SAMPLE_BY";
        case ASTAlterCommand::MODIFY_TTL:               return "MODIFY_TTL";
        case ASTAlterCommand::REWRITE_PARTS:            return "REWRITE_PARTS";
        case ASTAlterCommand::MATERIALIZE_TTL:          return "MATERIALIZE_TTL";
        case ASTAlterCommand::MODIFY_SETTING:           return "MODIFY_SETTING";
        case ASTAlterCommand::RESET_SETTING:            return "RESET_SETTING";
        case ASTAlterCommand::MODIFY_QUERY:             return "MODIFY_QUERY";
        case ASTAlterCommand::MODIFY_REFRESH:           return "MODIFY_REFRESH";
        case ASTAlterCommand::REMOVE_TTL:               return "REMOVE_TTL";
        case ASTAlterCommand::REMOVE_SAMPLE_BY:         return "REMOVE_SAMPLE_BY";
        case ASTAlterCommand::ADD_INDEX:                return "ADD_INDEX";
        case ASTAlterCommand::DROP_INDEX:               return "DROP_INDEX";
        case ASTAlterCommand::MATERIALIZE_INDEX:        return "MATERIALIZE_INDEX";
        case ASTAlterCommand::ADD_CONSTRAINT:           return "ADD_CONSTRAINT";
        case ASTAlterCommand::DROP_CONSTRAINT:          return "DROP_CONSTRAINT";
        case ASTAlterCommand::ADD_PROJECTION:           return "ADD_PROJECTION";
        case ASTAlterCommand::DROP_PROJECTION:          return "DROP_PROJECTION";
        case ASTAlterCommand::MATERIALIZE_PROJECTION:   return "MATERIALIZE_PROJECTION";
        case ASTAlterCommand::ADD_STATISTICS:           return "ADD_STATISTICS";
        case ASTAlterCommand::DROP_STATISTICS:          return "DROP_STATISTICS";
        case ASTAlterCommand::MODIFY_STATISTICS:        return "MODIFY_STATISTICS";
        case ASTAlterCommand::MATERIALIZE_STATISTICS:   return "MATERIALIZE_STATISTICS";
        case ASTAlterCommand::DROP_PARTITION:           return "DROP_PARTITION";
        case ASTAlterCommand::DROP_DETACHED_PARTITION:  return "DROP_DETACHED_PARTITION";
        case ASTAlterCommand::FORGET_PARTITION:         return "FORGET_PARTITION";
        case ASTAlterCommand::ATTACH_PARTITION:         return "ATTACH_PARTITION";
        case ASTAlterCommand::MOVE_PARTITION:           return "MOVE_PARTITION";
        case ASTAlterCommand::REPLACE_PARTITION:        return "REPLACE_PARTITION";
        case ASTAlterCommand::FETCH_PARTITION:          return "FETCH_PARTITION";
        case ASTAlterCommand::FREEZE_PARTITION:         return "FREEZE_PARTITION";
        case ASTAlterCommand::FREEZE_ALL:               return "FREEZE_ALL";
        case ASTAlterCommand::UNFREEZE_PARTITION:       return "UNFREEZE_PARTITION";
        case ASTAlterCommand::UNFREEZE_ALL:             return "UNFREEZE_ALL";
        case ASTAlterCommand::DELETE:                   return "DELETE";
        case ASTAlterCommand::UPDATE:                   return "UPDATE";
        case ASTAlterCommand::APPLY_DELETED_MASK:       return "APPLY_DELETED_MASK";
        case ASTAlterCommand::APPLY_PATCHES:            return "APPLY_PATCHES";
        case ASTAlterCommand::NO_TYPE:                  return "NO_TYPE";
        case ASTAlterCommand::MODIFY_DATABASE_SETTING:  return "MODIFY_DATABASE_SETTING";
        case ASTAlterCommand::MODIFY_DATABASE_COMMENT:  return "MODIFY_DATABASE_COMMENT";
        case ASTAlterCommand::MODIFY_COMMENT:           return "MODIFY_COMMENT";
        case ASTAlterCommand::MODIFY_SQL_SECURITY:      return "MODIFY_SQL_SECURITY";
        case ASTAlterCommand::UNLOCK_SNAPSHOT:          return "UNLOCK_SNAPSHOT";
    }
    return "";
}

const char * killTypeToString(ASTKillQueryQuery::Type type)
{
    switch (type)
    {
        case ASTKillQueryQuery::Type::Query:           return "QUERY";
        case ASTKillQueryQuery::Type::Mutation:        return "MUTATION";
        case ASTKillQueryQuery::Type::PartMoveToShard: return "PART_MOVE_TO_SHARD";
        case ASTKillQueryQuery::Type::Transaction:     return "TRANSACTION";
    }
    return "";
}

const char * dropKindToString(ASTDropQuery::Kind kind)
{
    switch (kind)
    {
        case ASTDropQuery::Kind::Drop:     return "DROP";
        case ASTDropQuery::Kind::Detach:   return "DETACH";
        case ASTDropQuery::Kind::Truncate: return "TRUNCATE";
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

/// Overloads for the raw `IAST *` / `ASTExpressionList *` members that DDL
/// nodes hold (alongside `children`) instead of `ASTPtr`s.
void addNodeSlot(JSONBuilder::JSONMap & node, const char * key, const IAST * child)
{
    if (child)
        node.add(key, formatASTAsJSON(*child));
}

JSONBuilder::ItemPtr inlineExpressionList(const IAST * list)
{
    auto array = std::make_unique<JSONBuilder::JSONArray>();
    if (list)
        for (const auto & child : list->children)
            array->add(formatASTAsJSON(*child));
    return array;
}

/// Common `database` / `table` / `temporary` slots shared by the table-scoped
/// DDL/DML statements (DROP, OPTIMIZE, DELETE, UPDATE, ...).
void addTableTarget(JSONBuilder::JSONMap & node, const ASTQueryWithTableAndOutput & query)
{
    if (query.isTemporary())
        node.add("temporary", true);
    addNodeSlot(node, "database", query.database);
    addNodeSlot(node, "table", query.table);
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

        /// Inline the `list_of_selects` wrapper so the operand selects appear
        /// directly under `selects`.
        node.add("selects", inlineExpressionList(select_union->list_of_selects));

        return true;
    }
    else if (const auto * subquery = dynamic_cast<const ASTSubquery *>(&ast))
    {
        if (!subquery->cte_name.empty())
            node.add("cte_name", subquery->cte_name);

        /// A subquery wraps exactly one query node.
        if (!subquery->children.empty())
            node.add("query", formatASTAsJSON(*subquery->children.front()));

        return true;
    }
    else if (const auto * with_element = dynamic_cast<const ASTWithElement *>(&ast))
    {
        node.add("name", with_element->name);
        addNodeSlot(node, "subquery", with_element->subquery);
        addNodeSlot(node, "aliases", with_element->aliases);

        return true;
    }
    else if (const auto * table_element = dynamic_cast<const ASTTablesInSelectQueryElement *>(&ast))
    {
        addNodeSlot(node, "table_join", table_element->table_join);
        addNodeSlot(node, "table_expression", table_element->table_expression);
        addNodeSlot(node, "array_join", table_element->array_join);

        return true;
    }
    else if (const auto * table_expression = dynamic_cast<const ASTTableExpression *>(&ast))
    {
        /// Exactly one of these three identifies the table.
        addNodeSlot(node, "database_and_table_name", table_expression->database_and_table_name);
        addNodeSlot(node, "table_function", table_expression->table_function);
        addNodeSlot(node, "subquery", table_expression->subquery);

        if (table_expression->final)
            node.add("final", true);
        addNodeSlot(node, "sample_size", table_expression->sample_size);
        addNodeSlot(node, "sample_offset", table_expression->sample_offset);
        addNodeSlot(node, "column_aliases", table_expression->column_aliases);

        return true;
    }
    else if (const auto * table_join = dynamic_cast<const ASTTableJoin *>(&ast))
    {
        node.add("kind", String(toString(table_join->kind)));
        if (table_join->strictness != JoinStrictness::Unspecified)
            node.add("strictness", String(toString(table_join->strictness)));
        if (table_join->locality != JoinLocality::Unspecified)
            node.add("locality", String(toString(table_join->locality)));

        if (table_join->using_expression_list)
            node.add("using", inlineExpressionList(table_join->using_expression_list));
        addNodeSlot(node, "on", table_join->on_expression);

        return true;
    }
    else if (const auto * array_join = dynamic_cast<const ASTArrayJoin *>(&ast))
    {
        node.add("kind", String(array_join->kind == ASTArrayJoin::Kind::Left ? "LEFT" : "INNER"));
        if (array_join->expression_list)
            node.add("expressions", inlineExpressionList(array_join->expression_list));

        return true;
    }
    else if (const auto * window_list_element = dynamic_cast<const ASTWindowListElement *>(&ast))
    {
        node.add("name", window_list_element->name);
        addNodeSlot(node, "definition", window_list_element->definition);

        return true;
    }
    else if (const auto * window_definition = dynamic_cast<const ASTWindowDefinition *>(&ast))
    {
        if (!window_definition->parent_window_name.empty())
            node.add("parent_window_name", window_definition->parent_window_name);

        if (window_definition->partition_by)
            node.add("partition_by", inlineExpressionList(window_definition->partition_by));
        if (window_definition->order_by)
            node.add("order_by", inlineExpressionList(window_definition->order_by));

        /// The frame is only meaningful when it differs from the implicit default.
        if (!window_definition->frame_is_default)
        {
            node.add("frame_type", String(windowFrameTypeToString(window_definition->frame_type)));

            node.add("frame_begin_type", String(windowBoundaryTypeToString(window_definition->frame_begin_type)));
            addNodeSlot(node, "frame_begin_offset", window_definition->frame_begin_offset);
            node.add("frame_begin_preceding", window_definition->frame_begin_preceding);

            node.add("frame_end_type", String(windowBoundaryTypeToString(window_definition->frame_end_type)));
            addNodeSlot(node, "frame_end_offset", window_definition->frame_end_offset);
            node.add("frame_end_preceding", window_definition->frame_end_preceding);
        }

        return true;
    }
    else if (const auto * interpolate = dynamic_cast<const ASTInterpolateElement *>(&ast))
    {
        node.add("column", interpolate->column);
        addNodeSlot(node, "expr", interpolate->expr);

        return true;
    }
    else if (const auto * set_query = dynamic_cast<const ASTSetQuery *>(&ast))
    {
        /// The SETTINGS clause: expose the changes as a name -> value object.
        if (!set_query->changes.empty())
        {
            auto changes = std::make_unique<JSONBuilder::JSONMap>();
            for (const auto & change : set_query->changes)
                changes->add(change.name, fieldToJSON(change.value));
            node.add("changes", std::move(changes));
        }

        if (!set_query->default_settings.empty())
        {
            auto defaults = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & name : set_query->default_settings)
                defaults->add(name);
            node.add("default_settings", std::move(defaults));
        }

        return true;
    }
    else if (const auto * sample_ratio = dynamic_cast<const ASTSampleRatio *>(&ast))
    {
        /// Kept as an exact rational; the components can exceed UInt64, so they
        /// are emitted as strings.
        node.add("numerator", String(ASTSampleRatio::toString(sample_ratio->ratio.numerator)));
        node.add("denominator", String(ASTSampleRatio::toString(sample_ratio->ratio.denominator)));

        return true;
    }
    else if (const auto * asterisk = dynamic_cast<const ASTAsterisk *>(&ast))
    {
        addNodeSlot(node, "expression", asterisk->expression);
        addNodeSlot(node, "transformers", asterisk->transformers);

        return true;
    }
    else if (const auto * qualified_asterisk = dynamic_cast<const ASTQualifiedAsterisk *>(&ast))
    {
        addNodeSlot(node, "qualifier", qualified_asterisk->qualifier);
        addNodeSlot(node, "transformers", qualified_asterisk->transformers);

        return true;
    }
    else if (const auto * regexp_matcher = dynamic_cast<const ASTColumnsRegexpMatcher *>(&ast))
    {
        node.add("pattern", regexp_matcher->getPattern());
        addNodeSlot(node, "expression", regexp_matcher->expression);
        addNodeSlot(node, "transformers", regexp_matcher->transformers);

        return true;
    }
    else if (const auto * list_matcher = dynamic_cast<const ASTColumnsListMatcher *>(&ast))
    {
        addNodeSlot(node, "expression", list_matcher->expression);
        if (list_matcher->column_list)
            node.add("columns", inlineExpressionList(list_matcher->column_list));
        addNodeSlot(node, "transformers", list_matcher->transformers);

        return true;
    }
    else if (const auto * apply = dynamic_cast<const ASTColumnsApplyTransformer *>(&ast))
    {
        if (!apply->func_name.empty())
            node.add("func_name", apply->func_name);
        addNodeSlot(node, "parameters", apply->parameters);
        addNodeSlot(node, "lambda", apply->lambda);
        if (!apply->lambda_arg.empty())
            node.add("lambda_arg", apply->lambda_arg);
        if (!apply->column_name_prefix.empty())
            node.add("column_name_prefix", apply->column_name_prefix);

        return true;
    }
    else if (const auto * except = dynamic_cast<const ASTColumnsExceptTransformer *>(&ast))
    {
        /// The excepted columns stay in `children` as a homogeneous list.
        if (except->is_strict)
            node.add("is_strict", true);
    }
    else if (const auto * replace = dynamic_cast<const ASTColumnsReplaceTransformer *>(&ast))
    {
        /// The replacements stay in `children` as a homogeneous list.
        if (replace->is_strict)
            node.add("is_strict", true);
    }
    else if (const auto * replacement = dynamic_cast<const ASTColumnsReplaceTransformer::Replacement *>(&ast))
    {
        /// The replacement expression stays as the single child.
        node.add("name", replacement->name);
    }
    else if (const auto * data_type = dynamic_cast<const ASTDataType *>(&ast))
    {
        node.add("name", data_type->name);
        if (auto arguments = data_type->getArguments())
            node.add("arguments", inlineExpressionList(arguments));

        return true;
    }
    else if (const auto * create = dynamic_cast<const ASTCreateQuery *>(&ast))
    {
        if (create->attach)
            node.add("attach", true);
        if (create->isTemporary())
            node.add("temporary", true);
        if (create->if_not_exists)
            node.add("if_not_exists", true);
        if (create->is_ordinary_view)
            node.add("is_ordinary_view", true);
        if (create->is_materialized_view)
            node.add("is_materialized_view", true);
        if (create->is_window_view)
            node.add("is_window_view", true);
        if (create->is_dictionary)
            node.add("is_dictionary", true);
        if (create->is_populate)
            node.add("is_populate", true);
        if (create->is_create_empty)
            node.add("is_create_empty", true);
        if (create->is_clone_as)
            node.add("is_clone_as", true);
        if (create->replace_view)
            node.add("replace_view", true);
        if (create->replace_table)
            node.add("replace_table", true);
        if (create->create_or_replace)
            node.add("create_or_replace", true);

        addNodeSlot(node, "database", create->database);
        addNodeSlot(node, "table", create->table);

        addNodeSlot(node, "columns_list", create->columns_list);
        if (create->aliases_list)
            node.add("aliases", inlineExpressionList(create->aliases_list));
        addNodeSlot(node, "storage", create->storage);
        addNodeSlot(node, "as_table_function", create->as_table_function);
        if (!create->as_database.empty())
            node.add("as_database", create->as_database);
        if (!create->as_table.empty())
            node.add("as_table", create->as_table);
        addNodeSlot(node, "select", create->select);
        addNodeSlot(node, "targets", create->targets);
        addNodeSlot(node, "comment", create->comment);
        if (create->dictionary_attributes_list)
            node.add("dictionary_attributes", inlineExpressionList(create->dictionary_attributes_list));
        addNodeSlot(node, "dictionary", create->dictionary);

        return true;
    }
    else if (const auto * columns = dynamic_cast<const ASTColumns *>(&ast))
    {
        if (columns->columns)
            node.add("columns", inlineExpressionList(columns->columns));
        if (columns->indices)
            node.add("indices", inlineExpressionList(columns->indices));
        if (columns->constraints)
            node.add("constraints", inlineExpressionList(columns->constraints));
        if (columns->projections)
            node.add("projections", inlineExpressionList(columns->projections));
        addNodeSlot(node, "primary_key", columns->primary_key);
        addNodeSlot(node, "primary_key_from_columns", columns->primary_key_from_columns);

        return true;
    }
    else if (const auto * column = dynamic_cast<const ASTColumnDeclaration *>(&ast))
    {
        node.add("name", column->name);
        addNodeSlot(node, "data_type", column->getType());

        if (column->default_specifier != ColumnDefaultSpecifier::Empty)
            node.add("default_specifier", String(toString(column->default_specifier)));
        addNodeSlot(node, "default_expression", column->getDefaultExpression());

        if (column->null_modifier.has_value())
            node.add("null_modifier", *column->null_modifier);
        if (column->ephemeral_default)
            node.add("ephemeral_default", true);
        if (column->primary_key_specifier)
            node.add("primary_key_specifier", true);

        addNodeSlot(node, "comment", column->getComment());
        addNodeSlot(node, "codec", column->getCodec());
        addNodeSlot(node, "statistics", column->getStatisticsDesc());
        addNodeSlot(node, "ttl", column->getTTL());
        addNodeSlot(node, "collation", column->getCollation());
        addNodeSlot(node, "settings", column->getSettings());

        return true;
    }
    else if (const auto * storage = dynamic_cast<const ASTStorage *>(&ast))
    {
        addNodeSlot(node, "engine", storage->engine);
        addNodeSlot(node, "partition_by", storage->partition_by);
        addNodeSlot(node, "primary_key", storage->primary_key);
        addNodeSlot(node, "order_by", storage->order_by);
        addNodeSlot(node, "sample_by", storage->sample_by);
        addNodeSlot(node, "ttl_table", storage->ttl_table);
        addNodeSlot(node, "settings", storage->settings);

        return true;
    }
    else if (const auto * insert = dynamic_cast<const ASTInsertQuery *>(&ast))
    {
        addNodeSlot(node, "database", insert->database);
        addNodeSlot(node, "table", insert->table);
        addNodeSlot(node, "table_function", insert->table_function);
        if (insert->columns)
            node.add("columns", inlineExpressionList(insert->columns));
        if (!insert->format.empty())
            node.add("format", insert->format);
        addNodeSlot(node, "partition_by", insert->partition_by);
        addNodeSlot(node, "settings", insert->settings_ast);
        addNodeSlot(node, "select", insert->select);
        addNodeSlot(node, "infile", insert->infile);
        addNodeSlot(node, "compression", insert->compression);

        return true;
    }
    else if (const auto * index = dynamic_cast<const ASTIndexDeclaration *>(&ast))
    {
        node.add("name", index->name);
        addNodeSlot(node, "expression", index->getExpression());
        addNodeSlot(node, "index_type", index->getType().get());
        node.add("granularity", index->granularity);

        return true;
    }
    else if (const auto * constraint = dynamic_cast<const ASTConstraintDeclaration *>(&ast))
    {
        node.add("name", constraint->name);
        node.add("constraint_type", String(constraintTypeToString(constraint->type)));
        addNodeSlot(node, "expression", constraint->expr);

        return true;
    }
    else if (const auto * projection = dynamic_cast<const ASTProjectionDeclaration *>(&ast))
    {
        node.add("name", projection->name);
        addNodeSlot(node, "query", projection->query);
        addNodeSlot(node, "index", projection->index);

        return true;
    }
    else if (const auto * projection_select = dynamic_cast<const ASTProjectionSelectQuery *>(&ast))
    {
        if (auto with_list = projection_select->with())
            node.add("with", inlineExpressionList(with_list));
        if (auto select_list = projection_select->select())
            node.add("select", inlineExpressionList(select_list));
        if (auto group_by_list = projection_select->groupBy())
            node.add("group_by", inlineExpressionList(group_by_list));
        if (auto order_by_list = projection_select->orderBy())
            node.add("order_by", inlineExpressionList(order_by_list));

        return true;
    }
    else if (const auto * ttl = dynamic_cast<const ASTTTLElement *>(&ast))
    {
        node.add("mode", String(ttlModeToString(ttl->mode)));
        addNodeSlot(node, "ttl", ttl->ttl());

        if (ttl->mode == TTLMode::MOVE)
        {
            node.add("destination_type", String(dataDestinationTypeToString(ttl->destination_type)));
            if (!ttl->destination_name.empty())
                node.add("destination_name", ttl->destination_name);
            if (ttl->if_exists)
                node.add("if_exists", true);
        }
        addNodeSlot(node, "where", ttl->where());
        addNodeSlot(node, "recompression_codec", ttl->recompression_codec);

        return true;
    }
    else if (const auto * partition = dynamic_cast<const ASTPartition *>(&ast))
    {
        if (partition->all)
            node.add("all", true);
        addNodeSlot(node, "value", partition->value);
        addNodeSlot(node, "id", partition->id);

        return true;
    }
    else if (const auto * assignment = dynamic_cast<const ASTAssignment *>(&ast))
    {
        node.add("column", assignment->column_name);
        addNodeSlot(node, "expression", assignment->expression());

        return true;
    }
    else if (const auto * del = dynamic_cast<const ASTDeleteQuery *>(&ast))
    {
        addTableTarget(node, *del);
        if (!del->cluster.empty())
            node.add("cluster", del->cluster);
        addNodeSlot(node, "partition", del->partition);
        addNodeSlot(node, "predicate", del->predicate);

        return true;
    }
    else if (const auto * update = dynamic_cast<const ASTUpdateQuery *>(&ast))
    {
        addTableTarget(node, *update);
        if (!update->cluster.empty())
            node.add("cluster", update->cluster);
        if (update->assignments)
            node.add("assignments", inlineExpressionList(update->assignments));
        addNodeSlot(node, "predicate", update->predicate);
        addNodeSlot(node, "partition", update->partition);

        return true;
    }
    else if (const auto * drop = dynamic_cast<const ASTDropQuery *>(&ast))
    {
        node.add("kind", String(dropKindToString(drop->kind)));
        addTableTarget(node, *drop);
        if (!drop->cluster.empty())
            node.add("cluster", drop->cluster);
        if (drop->if_exists)
            node.add("if_exists", true);
        if (drop->if_empty)
            node.add("if_empty", true);
        if (drop->is_dictionary)
            node.add("is_dictionary", true);
        if (drop->is_view)
            node.add("is_view", true);
        if (drop->sync)
            node.add("sync", true);
        if (drop->permanently)
            node.add("permanently", true);
        addNodeSlot(node, "database_and_tables", drop->database_and_tables);

        return true;
    }
    else if (const auto * optimize = dynamic_cast<const ASTOptimizeQuery *>(&ast))
    {
        addTableTarget(node, *optimize);
        if (!optimize->cluster.empty())
            node.add("cluster", optimize->cluster);
        addNodeSlot(node, "partition", optimize->partition);
        if (optimize->final)
            node.add("final", true);
        if (optimize->deduplicate)
            node.add("deduplicate", true);
        addNodeSlot(node, "deduplicate_by_columns", optimize->deduplicate_by_columns);
        if (optimize->cleanup)
            node.add("cleanup", true);

        return true;
    }
    else if (const auto * alter = dynamic_cast<const ASTAlterQuery *>(&ast))
    {
        if (alter->alter_object != ASTAlterQuery::AlterObjectType::UNKNOWN)
            node.add("alter_object", String(alterObjectTypeToString(alter->alter_object)));
        addTableTarget(node, *alter);
        if (!alter->cluster.empty())
            node.add("cluster", alter->cluster);
        if (alter->command_list)
            node.add("commands", inlineExpressionList(alter->command_list));

        return true;
    }
    else if (const auto * command = dynamic_cast<const ASTAlterCommand *>(&ast))
    {
        node.add("command_type", String(alterCommandTypeToString(command->type)));

        if (command->detach)
            node.add("detach", true);
        if (command->part)
            node.add("part", true);
        if (command->clear_column)
            node.add("clear_column", true);
        if (command->clear_index)
            node.add("clear_index", true);
        if (command->clear_projection)
            node.add("clear_projection", true);
        if (command->if_not_exists)
            node.add("if_not_exists", true);
        if (command->if_exists)
            node.add("if_exists", true);
        if (command->first)
            node.add("first", true);

        addNodeSlot(node, "column_declaration", command->col_decl);
        addNodeSlot(node, "column", command->column);
        addNodeSlot(node, "order_by", command->order_by);
        addNodeSlot(node, "sample_by", command->sample_by);
        addNodeSlot(node, "index_declaration", command->index_decl);
        addNodeSlot(node, "index", command->index);
        addNodeSlot(node, "constraint_declaration", command->constraint_decl);
        addNodeSlot(node, "constraint", command->constraint);
        addNodeSlot(node, "projection_declaration", command->projection_decl);
        addNodeSlot(node, "projection", command->projection);
        addNodeSlot(node, "statistics_declaration", command->statistics_decl);
        addNodeSlot(node, "partition", command->partition);
        addNodeSlot(node, "predicate", command->predicate);
        if (command->update_assignments)
            node.add("assignments", inlineExpressionList(command->update_assignments));
        addNodeSlot(node, "comment", command->comment);
        addNodeSlot(node, "ttl", command->ttl);
        addNodeSlot(node, "settings_changes", command->settings_changes);
        addNodeSlot(node, "settings_resets", command->settings_resets);
        addNodeSlot(node, "select", command->select);
        addNodeSlot(node, "sql_security", command->sql_security);
        addNodeSlot(node, "rename_to", command->rename_to);
        addNodeSlot(node, "refresh", command->refresh);

        if (!command->move_destination_name.empty())
            node.add("move_destination_name", command->move_destination_name);
        if (!command->from.empty())
            node.add("from", command->from);
        if (!command->from_database.empty())
            node.add("from_database", command->from_database);
        if (!command->from_table.empty())
            node.add("from_table", command->from_table);
        if (!command->to_database.empty())
            node.add("to_database", command->to_database);
        if (!command->to_table.empty())
            node.add("to_table", command->to_table);
        if (!command->remove_property.empty())
            node.add("remove_property", command->remove_property);

        return true;
    }
    else if (const auto * create_function = dynamic_cast<const ASTCreateFunctionQuery *>(&ast))
    {
        if (create_function->or_replace)
            node.add("or_replace", true);
        if (create_function->if_not_exists)
            node.add("if_not_exists", true);
        addNodeSlot(node, "function_name", create_function->function_name);
        addNodeSlot(node, "function_core", create_function->function_core);

        return true;
    }
    else if (const auto * dictionary = dynamic_cast<const ASTDictionary *>(&ast))
    {
        if (dictionary->primary_key)
            node.add("primary_key", inlineExpressionList(dictionary->primary_key));
        addNodeSlot(node, "source", dictionary->source);
        addNodeSlot(node, "lifetime", dictionary->lifetime);
        addNodeSlot(node, "layout", dictionary->layout);
        addNodeSlot(node, "range", dictionary->range);
        addNodeSlot(node, "settings", dictionary->dict_settings);

        return true;
    }
    else if (const auto * dict_layout = dynamic_cast<const ASTDictionaryLayout *>(&ast))
    {
        node.add("layout_type", dict_layout->layout_type);
        if (dict_layout->parameters)
            node.add("parameters", inlineExpressionList(dict_layout->parameters));

        return true;
    }
    else if (const auto * dict_lifetime = dynamic_cast<const ASTDictionaryLifetime *>(&ast))
    {
        node.add("min_sec", dict_lifetime->min_sec);
        node.add("max_sec", dict_lifetime->max_sec);

        return true;
    }
    else if (const auto * dict_range = dynamic_cast<const ASTDictionaryRange *>(&ast))
    {
        node.add("min_attr_name", dict_range->min_attr_name);
        node.add("max_attr_name", dict_range->max_attr_name);

        return true;
    }
    else if (const auto * dict_settings = dynamic_cast<const ASTDictionarySettings *>(&ast))
    {
        if (!dict_settings->changes.empty())
        {
            auto changes = std::make_unique<JSONBuilder::JSONMap>();
            for (const auto & change : dict_settings->changes)
                changes->add(change.name, fieldToJSON(change.value));
            node.add("changes", std::move(changes));
        }

        return true;
    }
    else if (const auto * dict_attr = dynamic_cast<const ASTDictionaryAttributeDeclaration *>(&ast))
    {
        node.add("name", dict_attr->name);
        addNodeSlot(node, "data_type", dict_attr->type);
        addNodeSlot(node, "default_value", dict_attr->default_value);
        addNodeSlot(node, "expression", dict_attr->expression);
        if (dict_attr->hierarchical)
            node.add("hierarchical", true);
        if (dict_attr->bidirectional)
            node.add("bidirectional", true);
        if (dict_attr->injective)
            node.add("injective", true);
        if (dict_attr->is_object_id)
            node.add("is_object_id", true);

        return true;
    }
    else if (const auto * kv = dynamic_cast<const ASTFunctionWithKeyValueArguments *>(&ast))
    {
        node.add("name", kv->name);
        if (kv->elements)
            node.add("elements", inlineExpressionList(kv->elements));

        return true;
    }
    else if (const auto * pair = dynamic_cast<const ASTPair *>(&ast))
    {
        node.add("key", pair->first);
        addNodeSlot(node, "value", pair->second);

        return true;
    }
    else if (const auto * view_targets = dynamic_cast<const ASTViewTargets *>(&ast))
    {
        auto targets = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & target : view_targets->targets)
        {
            auto entry = std::make_unique<JSONBuilder::JSONMap>();
            entry->add("kind", String(toString(target.kind)));
            if (!target.table_id.database_name.empty())
                entry->add("database", target.table_id.database_name);
            if (!target.table_id.table_name.empty())
                entry->add("table", target.table_id.table_name);
            addNodeSlot(*entry, "inner_engine", target.inner_engine);
            addNodeSlot(*entry, "table_ast", target.table_ast);
            targets->add(std::move(entry));
        }
        node.add("targets", std::move(targets));

        return true;
    }
    else if (const auto * explain = dynamic_cast<const ASTExplainQuery *>(&ast))
    {
        node.add("kind", ASTExplainQuery::toString(explain->getKind()));
        addNodeSlot(node, "query", explain->getExplainedQuery());
        addNodeSlot(node, "settings", explain->getSettings());
        addNodeSlot(node, "table_function", explain->getTableFunction());
        addNodeSlot(node, "table_override", explain->getTableOverride());

        return true;
    }
    else if (const auto * describe = dynamic_cast<const ASTDescribeQuery *>(&ast))
    {
        addNodeSlot(node, "table_expression", describe->table_expression);

        return true;
    }
    else if (const auto * show_tables = dynamic_cast<const ASTShowTablesQuery *>(&ast))
    {
        if (show_tables->databases)
            node.add("databases", true);
        if (show_tables->clusters)
            node.add("clusters", true);
        if (show_tables->dictionaries)
            node.add("dictionaries", true);
        if (show_tables->temporary)
            node.add("temporary", true);
        if (show_tables->full)
            node.add("full", true);
        addNodeSlot(node, "from", show_tables->from);
        if (!show_tables->like.empty())
            node.add("like", show_tables->like);
        if (show_tables->not_like)
            node.add("not_like", true);

        return true;
    }
    else if (const auto * create_index = dynamic_cast<const ASTCreateIndexQuery *>(&ast))
    {
        addTableTarget(node, *create_index);
        if (!create_index->cluster.empty())
            node.add("cluster", create_index->cluster);
        if (create_index->if_not_exists)
            node.add("if_not_exists", true);
        if (create_index->unique)
            node.add("unique", true);
        addNodeSlot(node, "index_name", create_index->index_name);
        addNodeSlot(node, "index_declaration", create_index->index_decl);

        return true;
    }
    else if (const auto * drop_index = dynamic_cast<const ASTDropIndexQuery *>(&ast))
    {
        addTableTarget(node, *drop_index);
        if (!drop_index->cluster.empty())
            node.add("cluster", drop_index->cluster);
        if (drop_index->if_exists)
            node.add("if_exists", true);
        addNodeSlot(node, "index_name", drop_index->index_name);

        return true;
    }
    else if (const auto * check = dynamic_cast<const ASTCheckTableQuery *>(&ast))
    {
        addTableTarget(node, *check);
        addNodeSlot(node, "partition", check->partition);
        if (!check->part_name.empty())
            node.add("part_name", check->part_name);

        return true;
    }
    else if (const auto * use = dynamic_cast<const ASTUseQuery *>(&ast))
    {
        addNodeSlot(node, "database", use->database);

        return true;
    }
    else if (const auto * kill = dynamic_cast<const ASTKillQueryQuery *>(&ast))
    {
        node.add("kill_type", String(killTypeToString(kill->type)));
        if (kill->sync)
            node.add("sync", true);
        if (kill->test)
            node.add("test", true);
        if (!kill->cluster.empty())
            node.add("cluster", kill->cluster);
        addNodeSlot(node, "where", kill->where_expression);

        return true;
    }
    else if (const auto * rename = dynamic_cast<const ASTRenameQuery *>(&ast))
    {
        if (rename->exchange)
            node.add("exchange", true);
        if (rename->database)
            node.add("database", true);
        if (rename->dictionary)
            node.add("dictionary", true);
        if (!rename->cluster.empty())
            node.add("cluster", rename->cluster);

        auto elements = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & element : rename->getElements())
        {
            auto entry = std::make_unique<JSONBuilder::JSONMap>();
            if (!element.from.getDatabase().empty())
                entry->add("from_database", element.from.getDatabase());
            if (!element.from.getTable().empty())
                entry->add("from_table", element.from.getTable());
            if (!element.to.getDatabase().empty())
                entry->add("to_database", element.to.getDatabase());
            if (!element.to.getTable().empty())
                entry->add("to_table", element.to.getTable());
            if (element.if_exists)
                entry->add("if_exists", true);
            elements->add(std::move(entry));
        }
        node.add("elements", std::move(elements));

        return true;
    }
    else if (const auto * system = dynamic_cast<const ASTSystemQuery *>(&ast))
    {
        node.add("system_type", String(ASTSystemQuery::typeToString(system->type)));
        addNodeSlot(node, "database", system->database);
        addNodeSlot(node, "table", system->table);
        if (!system->cluster.empty())
            node.add("cluster", system->cluster);
        if (!system->replica.empty())
            node.add("replica", system->replica);
        if (!system->shard.empty())
            node.add("shard", system->shard);
        if (!system->target_model.empty())
            node.add("target_model", system->target_model);
        if (!system->target_function.empty())
            node.add("target_function", system->target_function);

        return true;
    }
    else if (const auto * statistics = dynamic_cast<const ASTStatisticsDeclaration *>(&ast))
    {
        addNodeSlot(node, "columns", statistics->columns);
        addNodeSlot(node, "types", statistics->types);

        return true;
    }
    else if (const auto * storage_order_by = dynamic_cast<const ASTStorageOrderByElement *>(&ast))
    {
        node.add("direction", String(storage_order_by->direction >= 0 ? "ASC" : "DESC"));

        return false;  /// keep the sort expression in `children`
    }
    else if (const auto * name_type = dynamic_cast<const ASTNameTypePair *>(&ast))
    {
        node.add("name", name_type->name);
        addNodeSlot(node, "data_type", name_type->type);

        return true;
    }
    else if (const auto * q_regexp = dynamic_cast<const ASTQualifiedColumnsRegexpMatcher *>(&ast))
    {
        node.add("pattern", q_regexp->getPattern());
        addNodeSlot(node, "qualifier", q_regexp->qualifier);
        addNodeSlot(node, "transformers", q_regexp->transformers);

        return true;
    }
    else if (const auto * q_list = dynamic_cast<const ASTQualifiedColumnsListMatcher *>(&ast))
    {
        addNodeSlot(node, "qualifier", q_list->qualifier);
        if (q_list->column_list)
            node.add("columns", inlineExpressionList(q_list->column_list));
        addNodeSlot(node, "transformers", q_list->transformers);

        return true;
    }
    else if (const auto * table_query = dynamic_cast<const ASTQueryWithTableAndOutput *>(&ast))
    {
        /// Generic fallback for the simple table-scoped statements that carry
        /// nothing but a target (EXISTS, SHOW CREATE, UNDROP, ...). Must stay
        /// after every richer ASTQueryWithTableAndOutput subclass above.
        addTableTarget(node, *table_query);

        return true;
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
