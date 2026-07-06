#include <Parsers/DumpASTNode.h>

#include <Common/FieldVisitorToString.h>
#include <Parsers/ASTAsterisk.h>
#include <Parsers/ASTAlterQuery.h>
#include <Parsers/ASTAssignment.h>
#include <Parsers/ASTBackupQuery.h>
#include <Parsers/ASTCheckQuery.h>
#include <Parsers/ASTCreateIndexQuery.h>
#include <Parsers/ASTDropIndexQuery.h>
#include <Parsers/ASTExplainQuery.h>
#include <Parsers/ASTKillQueryQuery.h>
#include <Parsers/ASTNameTypePair.h>
#include <Parsers/ASTRenameQuery.h>
#include <Parsers/ASTShowColumnsQuery.h>
#include <Parsers/ASTShowFunctionsQuery.h>
#include <Parsers/ASTShowIndexesQuery.h>
#include <Parsers/ASTShowSettingQuery.h>
#include <Parsers/ASTShowTablesQuery.h>
#include <Parsers/ASTStatisticsDeclaration.h>
#include <Parsers/ASTSystemQuery.h>
#include <Parsers/ASTUndropQuery.h>
#include <Parsers/ASTUseQuery.h>
#include <Parsers/TablePropertiesQueriesASTs.h>
#include <Parsers/ASTCollation.h>
#include <Parsers/ASTColumnDeclaration.h>
#include <Parsers/ASTColumnsMatcher.h>
#include <Parsers/ASTColumnsTransformers.h>
#include <Parsers/ASTConstraintDeclaration.h>
#include <Parsers/ASTCreateFunctionQuery.h>
#include <Parsers/ASTCreateNamedCollectionQuery.h>
#include <Parsers/ASTCreateQuery.h>
#include <Parsers/ASTCreateResourceQuery.h>
#include <Parsers/ASTCreateWorkloadQuery.h>
#include <Parsers/ASTDataType.h>
#include <Parsers/ASTEnumDataType.h>
#include <Parsers/ASTObjectTypeArgument.h>
#include <Parsers/ASTRefreshStrategy.h>
#include <Parsers/ASTTimeInterval.h>
#include <Parsers/ASTTransactionControl.h>
#include <Parsers/ASTTupleDataType.h>
#include <Parsers/ASTDeleteQuery.h>
#include <Parsers/ASTDictionary.h>
#include <Parsers/ASTDictionaryAttributeDeclaration.h>
#include <Parsers/ASTFunctionWithKeyValueArguments.h>
#include <Parsers/ASTDropFunctionQuery.h>
#include <Parsers/ASTDropNamedCollectionQuery.h>
#include <Parsers/ASTDropResourceQuery.h>
#include <Parsers/ASTDropWorkloadQuery.h>
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
#include <Parsers/ASTQueryParameter.h>
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

#include <Parsers/Access/ASTAuthenticationData.h>
#include <Parsers/Access/ASTCheckGrantQuery.h>
#include <Parsers/Access/ASTCreateMaskingPolicyQuery.h>
#include <Parsers/Access/ASTCreateQuotaQuery.h>
#include <Parsers/Access/ASTCreateRoleQuery.h>
#include <Parsers/Access/ASTCreateRowPolicyQuery.h>
#include <Parsers/Access/ASTCreateSettingsProfileQuery.h>
#include <Parsers/Access/ASTCreateUserQuery.h>
#include <Parsers/Access/ASTDropAccessEntityQuery.h>
#include <Access/MaskingPolicy.h>
#include <Parsers/Access/ASTExecuteAsQuery.h>
#include <Parsers/Access/ASTGrantQuery.h>
#include <Parsers/Access/ASTMoveAccessEntityQuery.h>
#include <Parsers/Access/ASTPublicSSHKey.h>
#include <Parsers/Access/ASTRolesOrUsersSet.h>
#include <Parsers/Access/ASTRowPolicyName.h>
#include <Parsers/Access/ASTSetRoleQuery.h>
#include <Parsers/Access/ASTSettingsProfileElement.h>
#include <Parsers/Access/ASTShowAccessEntitiesQuery.h>
#include <Parsers/Access/ASTShowCreateAccessEntityQuery.h>
#include <Parsers/Access/ASTShowGrantsQuery.h>
#include <Parsers/Access/ASTUserNameWithHost.h>
#include <Parsers/ASTDatabaseOrNone.h>
#include <Access/Common/AccessEntityType.h>
#include <Access/Common/AccessFlags.h>
#include <Access/Common/AccessRightsElement.h>
#include <Access/Common/AllowedClientHosts.h>
#include <Access/Common/AuthenticationType.h>
#include <Access/Common/QuotaDefs.h>
#include <Access/Common/RowPolicyDefs.h>
#include <Common/SettingConstraintWritability.h>

#include <IO/Operators.h>


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

const char * syncReplicaModeToString(SyncReplicaMode mode)
{
    switch (mode)
    {
        case SyncReplicaMode::DEFAULT:     return "DEFAULT";
        case SyncReplicaMode::STRICT:      return "STRICT";
        case SyncReplicaMode::LIGHTWEIGHT: return "LIGHTWEIGHT";
        case SyncReplicaMode::PULL:        return "PULL";
    }
    return "";
}

const char * failPointActionToString(ASTSystemQuery::FailPointAction action)
{
    switch (action)
    {
        case ASTSystemQuery::FailPointAction::UNSPECIFIED: return "UNSPECIFIED";
        case ASTSystemQuery::FailPointAction::PAUSE:       return "PAUSE";
        case ASTSystemQuery::FailPointAction::RESUME:      return "RESUME";
    }
    return "";
}

const char * backupKindToString(ASTBackupQuery::Kind kind)
{
    switch (kind)
    {
        case ASTBackupQuery::Kind::BACKUP:  return "BACKUP";
        case ASTBackupQuery::Kind::RESTORE: return "RESTORE";
    }
    return "";
}

const char * backupElementTypeToString(ASTBackupQuery::ElementType type)
{
    switch (type)
    {
        case ASTBackupQuery::ElementType::TABLE:           return "TABLE";
        case ASTBackupQuery::ElementType::TEMPORARY_TABLE: return "TEMPORARY_TABLE";
        case ASTBackupQuery::ElementType::DATABASE:        return "DATABASE";
        case ASTBackupQuery::ElementType::ALL:             return "ALL";
    }
    return "";
}

const char * resourceAccessModeToString(ResourceAccessMode mode)
{
    switch (mode)
    {
        case ResourceAccessMode::DiskRead:     return "READ";
        case ResourceAccessMode::DiskWrite:    return "WRITE";
        case ResourceAccessMode::MasterThread: return "MASTER_THREAD";
        case ResourceAccessMode::WorkerThread: return "WORKER_THREAD";
        case ResourceAccessMode::Query:        return "QUERY";
    }
    return "";
}

const char * settingWritabilityToString(SettingConstraintWritability writability)
{
    switch (writability)
    {
        case SettingConstraintWritability::WRITABLE:               return "WRITABLE";
        case SettingConstraintWritability::CONST:                  return "CONST";
        case SettingConstraintWritability::CHANGEABLE_IN_READONLY: return "CHANGEABLE_IN_READONLY";
        case SettingConstraintWritability::MAX:                    return "";
    }
    return "";
}

const char * setRoleKindToString(ASTSetRoleQuery::Kind kind)
{
    switch (kind)
    {
        case ASTSetRoleQuery::Kind::SET_ROLE:         return "SET_ROLE";
        case ASTSetRoleQuery::Kind::SET_ROLE_DEFAULT: return "SET_ROLE_DEFAULT";
        case ASTSetRoleQuery::Kind::SET_DEFAULT_ROLE: return "SET_DEFAULT_ROLE";
    }
    return "";
}

const char * transactionActionToString(ASTTransactionControl::QueryType action)
{
    switch (action)
    {
        case ASTTransactionControl::BEGIN:        return "BEGIN";
        case ASTTransactionControl::COMMIT:       return "COMMIT";
        case ASTTransactionControl::ROLLBACK:     return "ROLLBACK";
        case ASTTransactionControl::SET_SNAPSHOT: return "SET_SNAPSHOT";
    }
    return "";
}

const char * refreshScheduleKindToString(RefreshScheduleKind kind)
{
    switch (kind)
    {
        case RefreshScheduleKind::UNKNOWN: return "UNKNOWN";
        case RefreshScheduleKind::AFTER:   return "AFTER";
        case RefreshScheduleKind::EVERY:   return "EVERY";
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

JSONBuilder::ItemPtr fieldToJSON(const Field & value);

/// Wrap a Field as a typed JSON object: { "value_type": <type>, "value": <value> }.
/// This mirrors how an `ASTLiteral` node is serialized, so that the element types
/// of nested containers (e.g. the `UInt64` elements of an `Array`) are preserved
/// rather than being flattened into an untyped string.
JSONBuilder::ItemPtr fieldToTypedJSON(const Field & value)
{
    auto map = std::make_unique<JSONBuilder::JSONMap>();
    map->add("value_type", String(fieldTypeName(value.getType())));
    map->add("value", fieldToJSON(value));
    return map;
}

/// Emit the elements of an Array/Tuple/Map (all backed by a FieldVector) as a JSON
/// array of typed values, preserving each element's individual type.
JSONBuilder::ItemPtr fieldVectorToJSON(const FieldVector & elements)
{
    auto array = std::make_unique<JSONBuilder::JSONArray>();
    for (const auto & element : elements)
        array->add(fieldToTypedJSON(element));
    return array;
}

/// Stringify a Map key for use as a JSON object key (JSON keys are always
/// strings). String keys are emitted verbatim; other scalar keys fall back to
/// their FieldVisitorToString form (e.g. an integer key becomes its digits).
String fieldMapKeyToString(const Field & key)
{
    if (key.getType() == Field::Types::String)
        return key.safeGet<String>();
    return applyVisitor(FieldVisitorToString(), key);
}

/// A Map field is physically a vector of two-element (key, value) Tuples.
/// Emit it as a JSON object keyed by the (stringified) map key with typed
/// values, mirroring the `{k: v}` SETTINGS map syntax. If any element is not a
/// two-element tuple (not expected for a real Map field), fall back to the
/// generic array-of-tuples form so no information is lost.
JSONBuilder::ItemPtr fieldMapToJSON(const Map & map)
{
    for (const auto & element : map)
    {
        if (element.getType() != Field::Types::Tuple || element.safeGet<Tuple>().size() != 2)
            return fieldVectorToJSON(map);
    }

    auto object = std::make_unique<JSONBuilder::JSONMap>();
    for (const auto & element : map)
    {
        const Tuple & kv = element.safeGet<Tuple>();
        object->add(fieldMapKeyToString(kv[0]), fieldToTypedJSON(kv[1]));
    }
    return object;
}

/// Map a Field to a JSONBuilder value.
/// Common scalar Field types are emitted as native JSON values. Array / Tuple
/// recurse to a JSON array of typed `{ "value_type", "value" }` elements; Map /
/// Object become a JSON object keyed by the (stringified) key with typed values.
/// Remaining exotic types fall back to a string produced by `FieldVisitorToString`.
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
            /// Emitted as a string: values above 2^53 lose precision when a
            /// JavaScript consumer runs them through JSON.parse (IEEE-754).
            return std::make_unique<JSONBuilder::JSONString>(std::to_string(value.safeGet<UInt64>()));
        case Field::Types::Int64:
            return std::make_unique<JSONBuilder::JSONString>(std::to_string(value.safeGet<Int64>()));
        case Field::Types::Float64:
            return std::make_unique<JSONBuilder::JSONNumber<Float64>>(value.safeGet<Float64>());
        case Field::Types::String:
            return std::make_unique<JSONBuilder::JSONString>(value.safeGet<String>());
        case Field::Types::Array:
            return fieldVectorToJSON(value.safeGet<Array>());
        case Field::Types::Tuple:
            return fieldVectorToJSON(value.safeGet<Tuple>());
        case Field::Types::Map:
            return fieldMapToJSON(value.safeGet<Map>());
        case Field::Types::Object:
        {
            /// Object is keyed by String, so emit a JSON object of typed values.
            auto map = std::make_unique<JSONBuilder::JSONMap>();
            for (const auto & [key, element] : value.safeGet<Object>())
                map->add(key, fieldToTypedJSON(element));
            return map;
        }
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

/// Emit a plain `ASTs` vector (not wrapped in an `ASTExpressionList`) as a JSON
/// array. Used by nodes that keep loose node lists in dedicated members, e.g.
/// `ASTTTLElement::group_by_key`.
JSONBuilder::ItemPtr inlineASTs(const ASTs & list)
{
    auto array = std::make_unique<JSONBuilder::JSONArray>();
    for (const auto & child : list)
        if (child)
            array->add(formatASTAsJSON(*child));
    return array;
}

/// The `INTO OUTFILE` / `FORMAT` part of the trailing output clause held in
/// the ASTQueryWithOutput base:
/// `[INTO OUTFILE <file> [APPEND | TRUNCATE] [AND STDOUT] [COMPRESSION <c>
/// [LEVEL <n>]]] [FORMAT <name>]`. All members are null-safe (each slot is
/// only emitted when present). The `format` name is emitted as a plain string
/// to match the `format` field used for INSERT; `out_file`, `compression` and
/// `compression_level` are literal nodes serialized recursively, and the
/// APPEND / TRUNCATE / AND STDOUT modifiers are exposed as boolean flags.
///
/// The trailing `SETTINGS` slot is *not* handled here: it is added by each
/// caller under `"settings"`, because the settings member differs per node
/// type (`settings_ast` for most, but `ASTExplainQuery` keeps a separate
/// EXPLAIN-level settings on that key). Callers that expose the base
/// `settings_ast` do `addNodeSlot(node, "settings", query.settings_ast)`.
void addOutfileAndFormat(JSONBuilder::JSONMap & node, const ASTQueryWithOutput & query)
{
    if (query.out_file)
    {
        node.add("out_file", formatASTAsJSON(*query.out_file));
        if (query.isOutfileAppend())
            node.add("outfile_append", true);
        if (query.isOutfileTruncate())
            node.add("outfile_truncate", true);
        if (query.isIntoOutfileWithStdout())
            node.add("outfile_with_stdout", true);
        addNodeSlot(node, "compression", query.compression);
        addNodeSlot(node, "compression_level", query.compression_level);
    }

    String format_name;
    if (tryGetIdentifierNameInto(query.format_ast, format_name))
        node.add("format", format_name);
}

/// Common `database` / `table` / `temporary` slots shared by the table-scoped
/// DDL/DML statements (DROP, OPTIMIZE, DELETE, UPDATE, ...).
void addTableTarget(JSONBuilder::JSONMap & node, const ASTQueryWithTableAndOutput & query)
{
    if (query.isTemporary())
        node.add("temporary", true);
    addNodeSlot(node, "database", query.database);
    addNodeSlot(node, "table", query.table);
    if (query.uuid != UUIDHelpers::Nil)
        node.add("uuid", toString(query.uuid));
    /// Trailing output options held in the ASTQueryWithOutput base.
    addNodeSlot(node, "settings", query.settings_ast);
    addOutfileAndFormat(node, query);
}

/// Serialize the privilege list of a GRANT / REVOKE / CHECK GRANT statement.
/// Each AccessRightsElement is one "... ON ..." line; the AccessFlags bitmask
/// is decoded to its keyword list (SELECT, UPDATE(col), ...) via toKeywords().
/// These live in a plain value vector, not in the AST `children`.
JSONBuilder::ItemPtr serializeAccessElements(const AccessRightsElements & elements)
{
    auto array = std::make_unique<JSONBuilder::JSONArray>();
    for (const auto & element : elements)
    {
        auto item = std::make_unique<JSONBuilder::JSONMap>();

        auto access_types = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & keyword : element.access_flags.toKeywords())
            access_types->add(String(keyword));
        item->add("access_types", std::move(access_types));

        if (!element.database.empty())
            item->add("database", element.database);
        if (element.default_database)
            item->add("default_database", true);
        if (!element.table.empty())
            item->add("table", element.table);
        if (!element.columns.empty())
        {
            auto columns = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & column : element.columns)
                columns->add(column);
            item->add("columns", std::move(columns));
        }
        if (!element.parameter.empty())
            item->add("parameter", element.parameter);
        if (element.wildcard)
            item->add("wildcard", true);
        if (element.grant_option)
            item->add("grant_option", true);
        if (element.is_partial_revoke)
            item->add("is_partial_revoke", true);

        array->add(std::move(item));
    }
    return array;
}

/// Serialize an AllowedClientHosts value object (the HOST clause of CREATE USER).
/// Like AccessRightsElements it is a plain value member, not an AST node.
JSONBuilder::ItemPtr serializeAllowedHosts(const AllowedClientHosts & hosts)
{
    auto map = std::make_unique<JSONBuilder::JSONMap>();
    if (hosts.containsAnyHost())
        map->add("any_host", true);
    if (hosts.containsLocalHost())
        map->add("local_host", true);

    auto add_strings = [&](const char * key, const Strings & values)
    {
        if (values.empty())
            return;
        auto array = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & value : values)
            array->add(value);
        map->add(key, std::move(array));
    };

    add_strings("names", hosts.getNames());
    add_strings("name_regexps", hosts.getNameRegexps());
    add_strings("like_patterns", hosts.getLikePatterns());

    if (!hosts.getAddresses().empty())
    {
        auto array = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & address : hosts.getAddresses())
            array->add(address.toString());
        map->add("addresses", std::move(array));
    }
    if (!hosts.getSubnets().empty())
    {
        auto array = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & subnet : hosts.getSubnets())
            array->add(subnet.toString());
        map->add("subnets", std::move(array));
    }
    return map;
}

/// Emit a plain list of strings under `key` (only when non-empty).
void addStringList(JSONBuilder::JSONMap & node, const char * key, const Strings & values)
{
    if (values.empty())
        return;
    auto array = std::make_unique<JSONBuilder::JSONArray>();
    for (const auto & value : values)
        array->add(value);
    node.add(key, std::move(array));
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
    else if (const auto * query_parameter = dynamic_cast<const ASTQueryParameter *>(&ast))
    {
        /// `{name:type}` — appears in value position and in identifier/table
        /// position. The substitution type is exposed as `param_type`.
        node.add("name", query_parameter->name);
        node.add("param_type", query_parameter->type);

        return true;
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
        /// Derives from ASTSelectQuery but keeps its operand selects in the
        /// positional `children` array rather than the `Expression` slots, so
        /// it must be matched *before* ASTSelectQuery. We expose those operands
        /// under `selects` (mirroring ASTSelectWithUnionQuery) and suppress
        /// `children`.
        if (intersect_except->final_operator != ASTSelectIntersectExceptQuery::Operator::UNKNOWN)
            node.add("operator", String(ASTSelectIntersectExceptQuery::fromOperator(intersect_except->final_operator)));
        node.add("selects", inlineExpressionList(intersect_except));

        return true;
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
        if (select->limit_with_ties)
            node.add("limit_with_ties", true);

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
            {Expression::WITH,         "with",        true},
            {Expression::SELECT,       "select",      true},
            {Expression::TABLES,       "from",        false},
            {Expression::PREWHERE,     "prewhere",    false},
            {Expression::WHERE,        "where",       false},
            {Expression::GROUP_BY,     "group_by",    true},
            {Expression::HAVING,       "having",      false},
            {Expression::WINDOW,       "window",      true},
            {Expression::QUALIFY,      "qualify",     false},
            {Expression::ORDER_BY,     "order_by",    true},
            {Expression::LIMIT_OFFSET, "offset",      false},
            {Expression::LIMIT_LENGTH, "limit",       false},
            {Expression::SETTINGS,     "settings",    false},
            {Expression::INTERPOLATE,  "interpolate", true},
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

        /// LIMIT ... BY ... is grouped into one object: {length, offset?, by}.
        if (auto by = select->getExpression(Expression::LIMIT_BY))
        {
            auto limit_by = std::make_unique<JSONBuilder::JSONMap>();
            if (auto length = select->getExpression(Expression::LIMIT_BY_LENGTH))
                limit_by->add("length", formatASTAsJSON(*length));
            if (auto offset = select->getExpression(Expression::LIMIT_BY_OFFSET))
                limit_by->add("offset", formatASTAsJSON(*offset));
            limit_by->add("by", inlineExpressionList(by));
            node.add("limit_by", std::move(limit_by));
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
        /// The trailing output clause (`INTO OUTFILE` / `FORMAT` / output-level
        /// `SETTINGS`, incl. on UNION / INTERSECT / EXCEPT) lives on this
        /// wrapper, not on the operand selects. The `SETTINGS` here is the one
        /// placed *after* `FORMAT` (`SELECT ... FORMAT x SETTINGS y`); a
        /// `SETTINGS` before `FORMAT` is consumed into the operand select's
        /// own `settings` slot instead.
        addNodeSlot(node, "settings", select_union->settings_ast);
        addOutfileAndFormat(node, *select_union);

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

        /// The frame is only meaningful when it differs from the implicit
        /// default. Each boundary is one object: {type, offset?, preceding?},
        /// `preceding` emitted only when true (per the flags-when-set contract).
        if (!window_definition->frame_is_default)
        {
            node.add("frame_type", String(windowFrameTypeToString(window_definition->frame_type)));

            auto begin = std::make_unique<JSONBuilder::JSONMap>();
            begin->add("type", String(windowBoundaryTypeToString(window_definition->frame_begin_type)));
            addNodeSlot(*begin, "offset", window_definition->frame_begin_offset);
            if (window_definition->frame_begin_preceding)
                begin->add("preceding", true);
            node.add("frame_begin", std::move(begin));

            auto end = std::make_unique<JSONBuilder::JSONMap>();
            end->add("type", String(windowBoundaryTypeToString(window_definition->frame_end_type)));
            addNodeSlot(*end, "offset", window_definition->frame_end_offset);
            if (window_definition->frame_end_preceding)
                end->add("preceding", true);
            node.add("frame_end", std::move(end));
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
        if (asterisk->transformers)
            node.add("transformers", inlineExpressionList(asterisk->transformers));

        return true;
    }
    else if (const auto * qualified_asterisk = dynamic_cast<const ASTQualifiedAsterisk *>(&ast))
    {
        addNodeSlot(node, "qualifier", qualified_asterisk->qualifier);
        if (qualified_asterisk->transformers)
            node.add("transformers", inlineExpressionList(qualified_asterisk->transformers));

        return true;
    }
    else if (const auto * regexp_matcher = dynamic_cast<const ASTColumnsRegexpMatcher *>(&ast))
    {
        node.add("pattern", regexp_matcher->getPattern());
        addNodeSlot(node, "expression", regexp_matcher->expression);
        if (regexp_matcher->transformers)
            node.add("transformers", inlineExpressionList(regexp_matcher->transformers));

        return true;
    }
    else if (const auto * list_matcher = dynamic_cast<const ASTColumnsListMatcher *>(&ast))
    {
        addNodeSlot(node, "expression", list_matcher->expression);
        if (list_matcher->column_list)
            node.add("columns", inlineExpressionList(list_matcher->column_list));
        if (list_matcher->transformers)
            node.add("transformers", inlineExpressionList(list_matcher->transformers));

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
        if (except->is_strict)
            node.add("is_strict", true);
        if (!except->children.empty())
            node.add("columns", inlineExpressionList(&ast));
        if (const auto & pattern = except->getPattern())
            node.add("pattern", *pattern);

        return true;
    }
    else if (const auto * replace = dynamic_cast<const ASTColumnsReplaceTransformer *>(&ast))
    {
        if (replace->is_strict)
            node.add("is_strict", true);
        if (!replace->children.empty())
            node.add("replacements", inlineExpressionList(&ast));

        return true;
    }
    else if (const auto * replacement = dynamic_cast<const ASTColumnsReplaceTransformer::Replacement *>(&ast))
    {
        node.add("name", replacement->name);
        if (!replacement->children.empty())
            node.add("expression", formatASTAsJSON(*replacement->children.front()));

        return true;
    }
    else if (const auto * enum_type = dynamic_cast<const ASTEnumDataType *>(&ast))
    {
        /// Must precede the ASTDataType branch: ASTEnumDataType derives from it,
        /// but its values live in a dedicated vector rather than in `getArguments`.
        node.add("name", enum_type->name);

        auto values = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & [enum_name, enum_value] : enum_type->values)
        {
            auto value_node = std::make_unique<JSONBuilder::JSONMap>();
            value_node->add("name", enum_name);
            value_node->add("value", enum_value);
            values->add(std::move(value_node));
        }
        node.add("values", std::move(values));

        return true;
    }
    else if (const auto * tuple_type = dynamic_cast<const ASTTupleDataType *>(&ast))
    {
        /// Must precede the ASTDataType branch (derived class). The element types
        /// are in `getArguments`; the names of a named tuple are stored separately
        /// in `element_names` (empty for an unnamed tuple).
        node.add("name", tuple_type->name);
        if (auto arguments = tuple_type->getArguments())
            node.add("arguments", inlineExpressionList(arguments));

        if (!tuple_type->element_names.empty())
        {
            auto names = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & elem_name : tuple_type->element_names)
                names->add(elem_name);
            node.add("element_names", std::move(names));
        }

        return true;
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
        if (create->has_attach_from_path)
            node.add("attach_from_path", create->attach_from_path);
        if (create->attach_as_replicated.has_value())
            node.add("attach_as_replicated", *create->attach_as_replicated);
        if (create->uuid != UUIDHelpers::Nil)
            node.add("uuid", toString(create->uuid));
        if (!create->cluster.empty())
            node.add("cluster", create->cluster);

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
        addNodeSlot(node, "refresh", create->refresh_strategy);
        addNodeSlot(node, "settings", create->settings_ast);
        addOutfileAndFormat(node, *create);

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
        /// The indexed-projection TYPE (PROJECTION p INDEX expr TYPE name) and the
        /// projection-level SETTINGS live in dedicated members, not in `children`.
        addNodeSlot(node, "index_type", static_cast<const IAST *>(projection->type));
        addNodeSlot(node, "settings", static_cast<const IAST *>(projection->with_settings));

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
        /// Unlike a normal SELECT (whose ORDER BY is an ExpressionList of
        /// OrderByElement), a projection stores ORDER BY as a *single* node:
        /// multiple keys are packed into a `tuple(...)` function and a lone key
        /// is kept bare (see ParserProjectionSelectQuery / formatImpl). Inlining
        /// its `children` directly drops a single key (a bare identifier has no
        /// children), and for the other cases leaks the argument-list wrapper —
        /// losing the function name of a `f(x)` key entirely. Mirror the
        /// formatter instead and expose the keys as a flat `order_by` list.
        if (auto projection_order_by = projection_select->orderBy())
        {
            if (const auto * tuple_key = projection_order_by->as<ASTFunction>(); tuple_key && tuple_key->name == "tuple" && tuple_key->arguments)
                node.add("order_by", inlineExpressionList(tuple_key->arguments));
            else
                node.add("order_by", inlineASTs(ASTs{projection_order_by}));
        }

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
        /// GROUP BY ... SET ... keeps its keys and assignments in dedicated
        /// members; the native AST otherwise retains only `mode: GROUP_BY`.
        if (ttl->mode == TTLMode::GROUP_BY)
        {
            if (!ttl->group_by_key.empty())
                node.add("group_by_key", inlineASTs(ttl->group_by_key));
            if (!ttl->group_by_assignments.empty())
                node.add("group_by_assignments", inlineASTs(ttl->group_by_assignments));
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
    else if (const auto * collation = dynamic_cast<const ASTCollation *>(&ast))
    {
        /// The collation name (COLLATE 'en_US') lives in the `collation` member,
        /// not in `children`, so without this branch the node would expose only
        /// its `type`. Applies to both column and ORDER BY collations.
        if (collation->collation)
        {
            if (auto name = tryGetIdentifierName(collation->collation))
                node.add("name", *name);
            else
                node.add("name", formatASTAsJSON(*collation->collation));
        }

        return true;
    }
    else if (const auto * typed_path = dynamic_cast<const ASTObjectTypedPathArgument *>(&ast))
    {
        /// JSON/Object typed path (a.b.c Type): the path name is kept in the
        /// `path` member (only echoed into getID, which astTypeName trims off).
        /// The slot for the type is `data_type` (not `type`, which is reserved
        /// for the node-class discriminator).
        node.add("name", typed_path->path);
        addNodeSlot(node, "data_type", typed_path->type);

        return true;
    }
    else if (const auto * object_arg = dynamic_cast<const ASTObjectTypeArgument *>(&ast))
    {
        /// An argument of a JSON/Object type: exactly one of a typed path, a
        /// skipped path, a skipped-path regexp, or a `setting = N` parameter.
        addNodeSlot(node, "path_with_type", object_arg->path_with_type);
        addNodeSlot(node, "skip_path", object_arg->skip_path);
        addNodeSlot(node, "skip_path_regexp", object_arg->skip_path_regexp);
        addNodeSlot(node, "parameter", object_arg->parameter);

        return true;
    }
    else if (const auto * tcl = dynamic_cast<const ASTTransactionControl *>(&ast))
    {
        /// Transaction statements collapse to one node type; the action and the
        /// snapshot value are otherwise lost.
        node.add("action", String(transactionActionToString(tcl->action)));
        if (tcl->action == ASTTransactionControl::SET_SNAPSHOT)
            node.add("snapshot", std::to_string(tcl->snapshot));

        return true;
    }
    else if (const auto * refresh = dynamic_cast<const ASTRefreshStrategy *>(&ast))
    {
        /// REFRESH ... strategy of a refreshable materialized view. The schedule
        /// kind and the APPEND flag are scalar members, not children.
        node.add("schedule_kind", String(refreshScheduleKindToString(refresh->schedule_kind)));
        if (refresh->append)
            node.add("append", true);
        addNodeSlot(node, "period", static_cast<const IAST *>(refresh->period));
        addNodeSlot(node, "offset", static_cast<const IAST *>(refresh->offset));
        addNodeSlot(node, "spread", static_cast<const IAST *>(refresh->spread));
        addNodeSlot(node, "dependencies", static_cast<const IAST *>(refresh->dependencies));
        addNodeSlot(node, "settings", static_cast<const IAST *>(refresh->settings));

        return true;
    }
    else if (const auto * interval = dynamic_cast<const ASTTimeInterval *>(&ast))
    {
        /// Compound time interval (1 YEAR 3 DAY 15 MINUTE). The whole value is
        /// held in the `interval` member with no AST children.
        auto units = std::make_unique<JSONBuilder::JSONArray>();
        for (auto [kind, value] : interval->interval.toIntervals())
        {
            auto entry = std::make_unique<JSONBuilder::JSONMap>();
            entry->add("kind", String(kind.toString()));
            entry->add("value", std::to_string(value));
            units->add(std::move(entry));
        }
        node.add("interval", std::move(units));

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
        if (drop->has_all)
            node.add("has_all", true);
        if (drop->has_tables)
            node.add("has_tables", true);
        if (!drop->like.empty())
        {
            node.add("like", drop->like);
            if (drop->not_like)
                node.add("not_like", true);
            if (drop->case_insensitive_like)
                node.add("case_insensitive_like", true);
        }
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
        if (command->clear_statistics)
            node.add("clear_statistics", true);
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

        /// move_destination_type is only initialized for MOVE PARTITION/PART.
        if (command->type == ASTAlterCommand::MOVE_PARTITION)
            node.add("move_destination_type", String(dataDestinationTypeToString(command->move_destination_type)));
        if (!command->move_destination_name.empty())
            node.add("move_destination_name", command->move_destination_name);
        if (!command->from.empty())
            node.add("from", command->from);
        if (!command->with_name.empty())
            node.add("with_name", command->with_name);
        /// Distinguishes REPLACE PARTITION ... FROM (true) from ATTACH PARTITION ... FROM (false).
        if (command->type == ASTAlterCommand::REPLACE_PARTITION)
            node.add("replace", command->replace);
        if (!command->from_database.empty())
            node.add("from_database", command->from_database);
        if (!command->from_table.empty())
            node.add("from_table", command->from_table);
        if (!command->to_database.empty())
            node.add("to_database", command->to_database);
        if (!command->to_table.empty())
            node.add("to_table", command->to_table);
        /// snapshot_name / snapshot_desc are only initialized for UNLOCK SNAPSHOT.
        if (command->type == ASTAlterCommand::UNLOCK_SNAPSHOT)
        {
            if (!command->snapshot_name.empty())
                node.add("snapshot_name", command->snapshot_name);
            addNodeSlot(node, "snapshot_desc", command->snapshot_desc);
        }
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
        if (!create_function->cluster.empty())
            node.add("cluster", create_function->cluster);
        addNodeSlot(node, "function_name", create_function->function_name);
        addNodeSlot(node, "function_core", create_function->function_core);

        return true;
    }
    else if (const auto * drop_function = dynamic_cast<const ASTDropFunctionQuery *>(&ast))
    {
        /// `function_name` / `cluster` are plain strings here (not child nodes),
        /// so without this branch the node would expose nothing but its `type`.
        node.add("function_name", drop_function->function_name);
        if (drop_function->if_exists)
            node.add("if_exists", true);
        if (!drop_function->cluster.empty())
            node.add("cluster", drop_function->cluster);

        return true;
    }
    else if (const auto * create_named_collection = dynamic_cast<const ASTCreateNamedCollectionQuery *>(&ast))
    {
        /// CREATE NAMED COLLECTION. Everything lives in plain members (the node
        /// has no `children`), so without this branch the JSON would expose
        /// only its `type`.
        node.add("collection_name", create_named_collection->collection_name);
        if (create_named_collection->if_not_exists)
            node.add("if_not_exists", true);
        if (!create_named_collection->cluster.empty())
            node.add("cluster", create_named_collection->cluster);

        /// The `key = value` body as a name -> value object. Unlike the
        /// `Settings` node (whose values are untyped strings), named-collection
        /// values are emitted as typed `{ "value_type", "value" }` pairs, just
        /// like an `ASTLiteral`. Per-key OVERRIDABLE / NOT OVERRIDABLE flags, when
        /// present, are surfaced separately under `overridability`.
        if (!create_named_collection->changes.empty())
        {
            auto changes = std::make_unique<JSONBuilder::JSONMap>();
            for (const auto & change : create_named_collection->changes)
                changes->add(change.name, fieldToTypedJSON(change.value));
            node.add("changes", std::move(changes));
        }
        if (!create_named_collection->overridability.empty())
        {
            auto overridability = std::make_unique<JSONBuilder::JSONMap>();
            for (const auto & [key, value] : create_named_collection->overridability)
                overridability->add(key, value);
            node.add("overridability", std::move(overridability));
        }

        return true;
    }
    else if (const auto * create_workload = dynamic_cast<const ASTCreateWorkloadQuery *>(&ast))
    {
        /// CREATE WORKLOAD. The name / parent identifiers live in `children`,
        /// but the SETTINGS changes (each with an optional `FOR resource`) are
        /// kept in a plain member, so handle the whole node explicitly.
        if (create_workload->or_replace)
            node.add("or_replace", true);
        if (create_workload->if_not_exists)
            node.add("if_not_exists", true);
        if (!create_workload->cluster.empty())
            node.add("cluster", create_workload->cluster);
        addNodeSlot(node, "workload_name", create_workload->workload_name);
        addNodeSlot(node, "workload_parent", create_workload->workload_parent);

        if (!create_workload->changes.empty())
        {
            auto changes = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & change : create_workload->changes)
            {
                auto item = std::make_unique<JSONBuilder::JSONMap>();
                item->add("name", change.name);
                item->add("value", fieldToTypedJSON(change.value));
                if (!change.resource.empty())
                    item->add("resource", change.resource);
                changes->add(std::move(item));
            }
            node.add("changes", std::move(changes));
        }

        return true;
    }
    else if (const auto * create_resource = dynamic_cast<const ASTCreateResourceQuery *>(&ast))
    {
        /// CREATE RESOURCE. The name identifier is in `children`, but the
        /// operation list and cost unit are plain members.
        if (create_resource->or_replace)
            node.add("or_replace", true);
        if (create_resource->if_not_exists)
            node.add("if_not_exists", true);
        if (!create_resource->cluster.empty())
            node.add("cluster", create_resource->cluster);
        addNodeSlot(node, "resource_name", create_resource->resource_name);
        node.add("unit", String(costUnitToString(create_resource->unit)));

        auto operations = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & operation : create_resource->operations)
        {
            auto item = std::make_unique<JSONBuilder::JSONMap>();
            item->add("mode", String(resourceAccessModeToString(operation.mode)));
            /// Absent `disk` means the operation applies to ALL disks.
            if (operation.disk)
                item->add("disk", *operation.disk);
            operations->add(std::move(item));
        }
        node.add("operations", std::move(operations));

        return true;
    }
    else if (const auto * drop_named_collection = dynamic_cast<const ASTDropNamedCollectionQuery *>(&ast))
    {
        /// DROP NAMED COLLECTION. `collection_name` / `cluster` are plain
        /// strings (not child nodes), so without this branch the node would
        /// expose nothing but its `type`.
        node.add("collection_name", drop_named_collection->collection_name);
        if (drop_named_collection->if_exists)
            node.add("if_exists", true);
        if (!drop_named_collection->cluster.empty())
            node.add("cluster", drop_named_collection->cluster);

        return true;
    }
    else if (const auto * drop_workload = dynamic_cast<const ASTDropWorkloadQuery *>(&ast))
    {
        /// DROP WORKLOAD. `workload_name` / `cluster` are plain strings (not
        /// child nodes), so without this branch the node would expose nothing
        /// but its `type`.
        node.add("workload_name", drop_workload->workload_name);
        if (drop_workload->if_exists)
            node.add("if_exists", true);
        if (!drop_workload->cluster.empty())
            node.add("cluster", drop_workload->cluster);

        return true;
    }
    else if (const auto * drop_resource = dynamic_cast<const ASTDropResourceQuery *>(&ast))
    {
        /// DROP RESOURCE. `resource_name` / `cluster` are plain strings (not
        /// child nodes), so without this branch the node would expose nothing
        /// but its `type`.
        node.add("resource_name", drop_resource->resource_name);
        if (drop_resource->if_exists)
            node.add("if_exists", true);
        if (!drop_resource->cluster.empty())
            node.add("cluster", drop_resource->cluster);

        return true;
    }
    else if (const auto * backup = dynamic_cast<const ASTBackupQuery *>(&ast))
    {
        /// BACKUP / RESTORE. The subject of the operation lives in `elements`
        /// (plain structs, not child nodes), so without this branch the JSON
        /// would expose only the destination function from `children`.
        node.add("kind", String(backupKindToString(backup->kind)));
        if (!backup->cluster.empty())
            node.add("cluster", backup->cluster);

        auto elements = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & element : backup->elements)
        {
            auto entry = std::make_unique<JSONBuilder::JSONMap>();
            entry->add("element_type", String(backupElementTypeToString(element.type)));

            if (!element.database_name.empty())
                entry->add("database", element.database_name);
            if (!element.table_name.empty())
                entry->add("table", element.table_name);

            /// `new_*` mirror the original names unless an `AS` clause renamed
            /// the object; only surface them when they actually differ.
            if (element.new_database_name != element.database_name)
                entry->add("new_database", element.new_database_name);
            if (element.new_table_name != element.table_name)
                entry->add("new_table", element.new_table_name);

            if (element.partitions)
            {
                auto partitions = std::make_unique<JSONBuilder::JSONArray>();
                for (const auto & partition_ast : *element.partitions)
                    partitions->add(formatASTAsJSON(*partition_ast));
                entry->add("partitions", std::move(partitions));
            }

            if (!element.except_tables.empty())
            {
                auto except_tables = std::make_unique<JSONBuilder::JSONArray>();
                for (const auto & [db, tbl] : element.except_tables)
                {
                    auto except_table = std::make_unique<JSONBuilder::JSONMap>();
                    if (!db.empty())
                        except_table->add("database", db);
                    except_table->add("table", tbl);
                    except_tables->add(std::move(except_table));
                }
                entry->add("except_tables", std::move(except_tables));
            }

            if (!element.except_databases.empty())
            {
                auto except_databases = std::make_unique<JSONBuilder::JSONArray>();
                for (const auto & db : element.except_databases)
                    except_databases->add(db);
                entry->add("except_databases", std::move(except_databases));
            }

            elements->add(std::move(entry));
        }
        node.add("elements", std::move(elements));

        /// `TO` / `FROM` destination, the optional incremental base, and the
        /// SETTINGS clause. `cluster_host_ids` is internal (populated only when
        /// rewriting an ON CLUSTER query), so it is normally absent.
        addNodeSlot(node, "backup_name", backup->backup_name);
        addNodeSlot(node, "base_backup_name", backup->base_backup_name);
        addNodeSlot(node, "settings", backup->settings);
        addNodeSlot(node, "cluster_host_ids", backup->cluster_host_ids);
        /// BACKUP/RESTORE derives from ASTQueryWithOutput, so it can carry a
        /// trailing FORMAT / INTO OUTFILE clause. (The SYNC/ASYNC wait mode is
        /// not a dedicated field — it is carried as the `async` setting inside
        /// the `settings` object above.)
        addOutfileAndFormat(node, *backup);

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
        /// EXPLAIN-level settings: `EXPLAIN SETTINGS <k>=<v> ...` (ASTExplainQuery::ast_settings).
        addNodeSlot(node, "settings", explain->getSettings());
        addNodeSlot(node, "table_function", explain->getTableFunction());
        addNodeSlot(node, "table_override", explain->getTableOverride());
        addOutfileAndFormat(node, *explain);
        /// Trailing output-clause settings: `EXPLAIN ... FORMAT <fmt> SETTINGS <k>=<v>`
        /// (the ASTQueryWithOutput base `settings_ast`). This is distinct from the
        /// EXPLAIN-level `settings` above, so it needs its own key. Every other
        /// ASTQueryWithOutput node maps `settings_ast` onto "settings", but Explain
        /// already uses that key for its EXPLAIN-level settings.
        addNodeSlot(node, "output_settings", explain->settings_ast);

        return true;
    }
    else if (const auto * describe = dynamic_cast<const ASTDescribeQuery *>(&ast))
    {
        addNodeSlot(node, "table_expression", describe->table_expression);
        addNodeSlot(node, "settings", describe->settings_ast);
        addOutfileAndFormat(node, *describe);

        return true;
    }
    else if (const auto * show_tables = dynamic_cast<const ASTShowTablesQuery *>(&ast))
    {
        /// SHOW TABLES / DATABASES / CLUSTERS / CLUSTER / DICTIONARIES /
        /// SETTINGS / MERGES / FILESYSTEM CACHES all collapse onto this one
        /// class; the flags below select which variant and carry its modifiers.
        if (show_tables->databases)
            node.add("databases", true);
        if (show_tables->clusters)
            node.add("clusters", true);
        /// `cluster` (singular) is SHOW CLUSTER <name>; the name lives in the
        /// plain `cluster_str` member, not in `children`. It is emitted as
        /// `cluster_name` — `cluster` on other nodes is the ON CLUSTER string,
        /// so reusing that key for the internal-style `_str` name would be
        /// confusing.
        if (show_tables->cluster)
            node.add("cluster", true);
        if (!show_tables->cluster_str.empty())
            node.add("cluster_name", show_tables->cluster_str);
        if (show_tables->dictionaries)
            node.add("dictionaries", true);
        /// The `m_settings` member (SHOW SETTINGS); emitted as `show_settings`
        /// so the internal `m_` prefix does not leak into the public format
        /// (a bare `settings` would collide with the settings slot).
        if (show_tables->m_settings)
            node.add("show_settings", true);
        if (show_tables->changed)
            node.add("changed", true);
        if (show_tables->merges)
            node.add("merges", true);
        if (show_tables->caches)
            node.add("caches", true);
        if (show_tables->temporary)
            node.add("temporary", true);
        if (show_tables->full)
            node.add("full", true);
        addNodeSlot(node, "from", show_tables->from);
        if (!show_tables->like.empty())
            node.add("like", show_tables->like);
        if (show_tables->not_like)
            node.add("not_like", true);
        /// LIKE vs ILIKE.
        if (show_tables->case_insensitive_like)
            node.add("case_insensitive_like", true);
        /// The WHERE filter and LIMIT (both plain child ASTs) are only used by
        /// the SHOW TABLES / DICTIONARIES variant, but surface them whenever set.
        addNodeSlot(node, "where", show_tables->where_expression);
        addNodeSlot(node, "limit", show_tables->limit_length);
        addNodeSlot(node, "settings", show_tables->settings_ast);
        addOutfileAndFormat(node, *show_tables);

        return true;
    }
    else if (const auto * show_columns = dynamic_cast<const ASTShowColumnsQuery *>(&ast))
    {
        /// SHOW [EXTENDED] [FULL] COLUMNS/FIELDS FROM <table> [FROM <db>]
        /// [(NOT) (I)LIKE <pattern> | WHERE <expr>] [LIMIT <n>]. ClickHouse's
        /// native AST keeps every operand in plain members (the `children`
        /// array is empty), so without this branch the JSON would collapse to
        /// bare {"type":"ShowColumns"} and the table, filters and modifiers
        /// would all be lost.
        if (show_columns->extended)
            node.add("extended", true);
        if (show_columns->full)
            node.add("full", true);
        /// `table` is mandatory (the parser requires `FROM <table>`); `database`
        /// is only present when the query named one (via `db.table` or a second
        /// `FROM <db>`).
        node.add("table", show_columns->table);
        if (!show_columns->database.empty())
            node.add("database", show_columns->database);
        if (!show_columns->like.empty())
        {
            node.add("like", show_columns->like);
            if (show_columns->not_like)
                node.add("not_like", true);
            /// LIKE vs ILIKE.
            if (show_columns->case_insensitive_like)
                node.add("case_insensitive_like", true);
        }
        /// WHERE and LIKE are mutually exclusive in the grammar, but both are
        /// plain child ASTs; surface each whenever set.
        addNodeSlot(node, "where", show_columns->where_expression);
        addNodeSlot(node, "limit", show_columns->limit_length);
        addNodeSlot(node, "settings", show_columns->settings_ast);
        addOutfileAndFormat(node, *show_columns);

        return true;
    }
    else if (const auto * show_setting = dynamic_cast<const ASTShowSettingQuery *>(&ast))
    {
        /// SHOW SETTING <name>. The setting name is the sole operand and lives
        /// in a private member, so without this branch the JSON would collapse
        /// to bare {"type":"ShowSetting"} and the name would be lost.
        node.add("setting_name", show_setting->getSettingName());
        addNodeSlot(node, "settings", show_setting->settings_ast);
        addOutfileAndFormat(node, *show_setting);

        return true;
    }
    else if (const auto * show_functions = dynamic_cast<const ASTShowFunctionsQuery *>(&ast))
    {
        /// SHOW FUNCTIONS [(I)LIKE <pattern>]. The pattern is the only operand
        /// and lives in a plain member (empty `children`), so without this
        /// branch the JSON would collapse to bare {"type":"ShowFunctions"} and
        /// the LIKE / ILIKE filter would be lost.
        if (!show_functions->like.empty())
        {
            node.add("like", show_functions->like);
            /// LIKE vs ILIKE.
            if (show_functions->case_insensitive_like)
                node.add("case_insensitive_like", true);
        }
        addNodeSlot(node, "settings", show_functions->settings_ast);
        addOutfileAndFormat(node, *show_functions);

        return true;
    }
    else if (const auto * show_indexes = dynamic_cast<const ASTShowIndexesQuery *>(&ast))
    {
        /// SHOW [EXTENDED] INDEX/INDEXES/INDICES/KEYS FROM <table> [FROM <db>]
        /// [WHERE <expr>]. A distinct class from ASTShowColumnsQuery that keeps
        /// every operand in plain members (empty `children`), so without this
        /// branch the JSON would collapse to bare {"type":"ShowIndexes"} and
        /// the table, database and WHERE filter would be lost. (Its `type` is
        /// disambiguated from ShowColumns in astTypeName — see below.)
        if (show_indexes->extended)
            node.add("extended", true);
        /// `table` is mandatory (the parser requires `FROM <table>`); `database`
        /// is only present when named (via `db.table` or a second `FROM <db>`).
        node.add("table", show_indexes->table);
        if (!show_indexes->database.empty())
            node.add("database", show_indexes->database);
        addNodeSlot(node, "where", show_indexes->where_expression);
        addNodeSlot(node, "settings", show_indexes->settings_ast);
        addOutfileAndFormat(node, *show_indexes);

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
        addNodeSlot(node, "settings", kill->settings_ast);
        addOutfileAndFormat(node, *kill);

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
        addNodeSlot(node, "settings", rename->settings_ast);

        return true;
    }
    else if (const auto * system = dynamic_cast<const ASTSystemQuery *>(&ast))
    {
        /// SYSTEM covers ~130 sub-commands; the operand fields below are each
        /// only meaningful for a subset, but they default to empty/zero and are
        /// surfaced whenever populated so every variant round-trips.
        node.add("system_type", String(ASTSystemQuery::typeToString(system->type)));
        addNodeSlot(node, "database", system->database);
        addNodeSlot(node, "table", system->table);
        if (system->if_exists)
            node.add("if_exists", true);
        if (!system->cluster.empty())
            node.add("cluster", system->cluster);
        if (!system->replica.empty())
            node.add("replica", system->replica);
        if (!system->shard.empty())
            node.add("shard", system->shard);
        if (!system->replica_zk_path.empty())
            node.add("replica_zk_path", system->replica_zk_path);
        if (system->is_drop_whole_replica)
            node.add("is_drop_whole_replica", true);
        if (system->with_tables)
            node.add("with_tables", true);
        if (!system->target_model.empty())
            node.add("target_model", system->target_model);
        if (!system->target_function.empty())
            node.add("target_function", system->target_function);
        if (!system->storage_policy.empty())
            node.add("storage_policy", system->storage_policy);
        if (!system->volume.empty())
            node.add("volume", system->volume);
        if (!system->disk.empty())
            node.add("disk", system->disk);
        /// Stringified like the other 64-bit scalars (precision-safe for JS consumers).
        if (system->seconds)
            node.add("seconds", std::to_string(system->seconds));
        /// SYNC REPLICA ... {STRICT|LIGHTWEIGHT|PULL} plus the optional
        /// LIGHTWEIGHT FROM <replicas> source list.
        if (system->sync_replica_mode != SyncReplicaMode::DEFAULT)
            node.add("sync_replica_mode", String(syncReplicaModeToString(system->sync_replica_mode)));
        addStringList(node, "src_replicas", system->src_replicas);
        /// FLUSH LOGS / FLUSH ASYNC INSERT QUEUE target list (`db.table` pairs).
        if (!system->tables.empty())
        {
            auto tables = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & [db, tbl] : system->tables)
            {
                auto item = std::make_unique<JSONBuilder::JSONMap>();
                if (!db.empty())
                    item->add("database", db);
                item->add("table", tbl);
                tables->add(std::move(item));
            }
            node.add("tables", std::move(tables));
        }
        addNodeSlot(node, "settings", system->query_settings);
        if (!system->schema_cache_storage.empty())
            node.add("schema_cache_storage", system->schema_cache_storage);
        if (!system->schema_cache_format.empty())
            node.add("schema_cache_format", system->schema_cache_format);
        if (!system->filesystem_cache_name.empty())
            node.add("filesystem_cache_name", system->filesystem_cache_name);
        if (!system->key_to_drop.empty())
            node.add("key_to_drop", system->key_to_drop);
        if (system->offset_to_drop.has_value())
            node.add("offset_to_drop", std::to_string(*system->offset_to_drop));
        if (system->distributed_cache_drop_connections)
            node.add("distributed_cache_drop_connections", true);
        if (!system->distributed_cache_server_id.empty())
            node.add("distributed_cache_server_id", system->distributed_cache_server_id);
        if (system->query_result_cache_tag.has_value())
            node.add("query_result_cache_tag", *system->query_result_cache_tag);
        if (!system->backup_name.empty())
            node.add("backup_name", system->backup_name);
        addNodeSlot(node, "backup_source", system->backup_source);
        if (!system->fail_point_name.empty())
            node.add("fail_point_name", system->fail_point_name);
        if (system->fail_point_action != ASTSystemQuery::FailPointAction::UNSPECIFIED)
            node.add("fail_point_action", String(failPointActionToString(system->fail_point_action)));
        /// SYSTEM TEST VIEW ... SET/UNSET FAKE TIME: absent value means UNSET.
        if (system->fake_time_for_view.has_value())
            node.add("fake_time_for_view", std::to_string(*system->fake_time_for_view));
        /// START/STOP LISTEN <server type> [EXCEPT ...]. `server_type.type` is
        /// only initialised for these two commands, so guard the access.
        if (system->type == ASTSystemQuery::Type::START_LISTEN
            || system->type == ASTSystemQuery::Type::STOP_LISTEN)
        {
            auto server_type = std::make_unique<JSONBuilder::JSONMap>();
            server_type->add("type", String(ServerType::serverTypeToString(system->server_type.type)));
            if (!system->server_type.custom_name.empty())
                server_type->add("custom_name", system->server_type.custom_name);
            if (!system->server_type.exclude_types.empty())
            {
                auto exclude_types = std::make_unique<JSONBuilder::JSONArray>();
                for (auto excluded : system->server_type.exclude_types)
                    exclude_types->add(String(ServerType::serverTypeToString(excluded)));
                server_type->add("exclude_types", std::move(exclude_types));
            }
            if (!system->server_type.exclude_custom_names.empty())
            {
                auto exclude_custom_names = std::make_unique<JSONBuilder::JSONArray>();
                for (const auto & excluded : system->server_type.exclude_custom_names)
                    exclude_custom_names->add(excluded);
                server_type->add("exclude_custom_names", std::move(exclude_custom_names));
            }
            node.add("server_type", std::move(server_type));
        }

        /// NOTE: the SYSTEM INSTRUMENT ADD/REMOVE operands are compiled only
        /// under USE_XRAY and are intentionally not serialized here.

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
        if (!storage_order_by->children.empty())
            node.add("expression", formatASTAsJSON(*storage_order_by->children.front()));
        node.add("direction", String(storage_order_by->direction >= 0 ? "ASC" : "DESC"));

        return true;
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
    else if (const auto * undrop = dynamic_cast<const ASTUndropQuery *>(&ast))
    {
        addTableTarget(node, *undrop);
        if (!undrop->cluster.empty())
            node.add("cluster", undrop->cluster);

        return true;
    }
    else if (const auto * grant = dynamic_cast<const ASTGrantQuery *>(&ast))
    {
        if (grant->attach_mode)
            node.add("attach_mode", true);
        if (grant->admin_option)
            node.add("admin_option", true);
        if (grant->current_grants)
            node.add("current_grants", true);
        if (grant->replace_access)
            node.add("replace_access", true);
        if (grant->replace_granted_roles)
            node.add("replace_granted_roles", true);
        if (!grant->cluster.empty())
            node.add("cluster", grant->cluster);

        /// Role-grant (GRANT role TO ...) uses `roles`; privilege-grant uses the
        /// AccessRightsElements value vector. They are mutually exclusive.
        if (grant->roles)
            addNodeSlot(node, "roles", grant->roles.get());
        else
            node.add("access_rights", serializeAccessElements(grant->access_rights_elements));

        addNodeSlot(node, "grantees", grant->grantees.get());

        return true;
    }
    else if (const auto * check_grant = dynamic_cast<const ASTCheckGrantQuery *>(&ast))
    {
        node.add("access_rights", serializeAccessElements(check_grant->access_rights_elements));

        return true;
    }
    else if (const auto * create_user = dynamic_cast<const ASTCreateUserQuery *>(&ast))
    {
        if (create_user->alter)
            node.add("alter", true);
        if (create_user->attach)
            node.add("attach", true);
        if (create_user->if_exists)
            node.add("if_exists", true);
        if (create_user->if_not_exists)
            node.add("if_not_exists", true);
        if (create_user->or_replace)
            node.add("or_replace", true);
        if (!create_user->cluster.empty())
            node.add("cluster", create_user->cluster);

        addNodeSlot(node, "names", create_user->names.get());
        if (create_user->new_name)
            node.add("new_name", *create_user->new_name);
        if (!create_user->storage_name.empty())
            node.add("storage_name", create_user->storage_name);

        if (!create_user->authentication_methods.empty())
        {
            auto methods = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & method : create_user->authentication_methods)
                methods->add(formatASTAsJSON(*method));
            node.add("authentication_methods", std::move(methods));
        }
        if (create_user->reset_authentication_methods_to_new)
            node.add("reset_authentication_methods_to_new", true);
        if (create_user->add_identified_with)
            node.add("add_identified_with", true);
        if (create_user->replace_authentication_methods)
            node.add("replace_authentication_methods", true);

        if (create_user->hosts)
            node.add("hosts", serializeAllowedHosts(*create_user->hosts));
        if (create_user->add_hosts)
            node.add("add_hosts", serializeAllowedHosts(*create_user->add_hosts));
        if (create_user->remove_hosts)
            node.add("remove_hosts", serializeAllowedHosts(*create_user->remove_hosts));

        addNodeSlot(node, "default_roles", create_user->default_roles.get());
        addNodeSlot(node, "default_database", create_user->default_database.get());
        addNodeSlot(node, "settings", create_user->settings.get());
        addNodeSlot(node, "alter_settings", create_user->alter_settings.get());
        addNodeSlot(node, "grantees", create_user->grantees.get());
        addNodeSlot(node, "global_valid_until", create_user->global_valid_until);

        return true;
    }
    else if (const auto * create_role = dynamic_cast<const ASTCreateRoleQuery *>(&ast))
    {
        if (create_role->alter)
            node.add("alter", true);
        if (create_role->attach)
            node.add("attach", true);
        if (create_role->if_exists)
            node.add("if_exists", true);
        if (create_role->if_not_exists)
            node.add("if_not_exists", true);
        if (create_role->or_replace)
            node.add("or_replace", true);
        if (!create_role->cluster.empty())
            node.add("cluster", create_role->cluster);

        addStringList(node, "names", create_role->names);
        if (!create_role->new_name.empty())
            node.add("new_name", create_role->new_name);
        if (!create_role->storage_name.empty())
            node.add("storage_name", create_role->storage_name);

        addNodeSlot(node, "settings", create_role->settings.get());
        addNodeSlot(node, "alter_settings", create_role->alter_settings.get());

        return true;
    }
    else if (const auto * create_quota = dynamic_cast<const ASTCreateQuotaQuery *>(&ast))
    {
        if (create_quota->alter)
            node.add("alter", true);
        if (create_quota->attach)
            node.add("attach", true);
        if (create_quota->if_exists)
            node.add("if_exists", true);
        if (create_quota->if_not_exists)
            node.add("if_not_exists", true);
        if (create_quota->or_replace)
            node.add("or_replace", true);
        if (!create_quota->cluster.empty())
            node.add("cluster", create_quota->cluster);

        addStringList(node, "names", create_quota->names);
        if (!create_quota->new_name.empty())
            node.add("new_name", create_quota->new_name);
        if (create_quota->key_type)
            node.add("key_type", toString(*create_quota->key_type));
        if (!create_quota->storage_name.empty())
            node.add("storage_name", create_quota->storage_name);

        if (!create_quota->all_limits.empty())
        {
            auto limits_array = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & limits : create_quota->all_limits)
            {
                auto limits_item = std::make_unique<JSONBuilder::JSONMap>();
                limits_item->add("duration_sec", std::to_string(limits.duration.count()));
                if (limits.randomize_interval)
                    limits_item->add("randomize_interval", true);
                if (limits.drop)
                    limits_item->add("drop", true);

                auto max_map = std::make_unique<JSONBuilder::JSONMap>();
                bool has_max = false;
                for (size_t i = 0; i < static_cast<size_t>(QuotaType::MAX); ++i)
                {
                    if (limits.max[i])
                    {
                        max_map->add(toString(static_cast<QuotaType>(i)), std::to_string(*limits.max[i]));
                        has_max = true;
                    }
                }
                if (has_max)
                    limits_item->add("max", std::move(max_map));

                limits_array->add(std::move(limits_item));
            }
            node.add("limits", std::move(limits_array));
        }

        addNodeSlot(node, "roles", create_quota->roles.get());

        return true;
    }
    else if (const auto * set_role = dynamic_cast<const ASTSetRoleQuery *>(&ast))
    {
        node.add("kind", String(setRoleKindToString(set_role->kind)));
        addNodeSlot(node, "roles", set_role->roles.get());
        addNodeSlot(node, "to_users", set_role->to_users.get());

        return true;
    }
    else if (const auto * row_policy = dynamic_cast<const ASTCreateRowPolicyQuery *>(&ast))
    {
        if (row_policy->alter)
            node.add("alter", true);
        if (row_policy->attach)
            node.add("attach", true);
        if (row_policy->if_exists)
            node.add("if_exists", true);
        if (row_policy->if_not_exists)
            node.add("if_not_exists", true);
        if (row_policy->or_replace)
            node.add("or_replace", true);
        if (!row_policy->cluster.empty())
            node.add("cluster", row_policy->cluster);
        if (!row_policy->storage_name.empty())
            node.add("storage_name", row_policy->storage_name);

        addNodeSlot(node, "names", row_policy->names.get());
        if (!row_policy->new_short_name.empty())
            node.add("new_short_name", row_policy->new_short_name);
        if (row_policy->is_restrictive)
            node.add("is_restrictive", *row_policy->is_restrictive);

        if (!row_policy->filters.empty())
        {
            auto filters = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & [filter_type, condition] : row_policy->filters)
            {
                auto filter_item = std::make_unique<JSONBuilder::JSONMap>();
                filter_item->add("filter_type", toString(filter_type));
                /// `nullptr` condition means the filter was set to NONE.
                if (condition)
                    filter_item->add("condition", formatASTAsJSON(*condition));
                filters->add(std::move(filter_item));
            }
            node.add("filters", std::move(filters));
        }

        addNodeSlot(node, "roles", row_policy->roles.get());

        return true;
    }
    else if (const auto * profile = dynamic_cast<const ASTCreateSettingsProfileQuery *>(&ast))
    {
        if (profile->alter)
            node.add("alter", true);
        if (profile->attach)
            node.add("attach", true);
        if (profile->if_exists)
            node.add("if_exists", true);
        if (profile->if_not_exists)
            node.add("if_not_exists", true);
        if (profile->or_replace)
            node.add("or_replace", true);
        if (!profile->cluster.empty())
            node.add("cluster", profile->cluster);
        if (!profile->storage_name.empty())
            node.add("storage_name", profile->storage_name);

        addStringList(node, "names", profile->names);
        if (!profile->new_name.empty())
            node.add("new_name", profile->new_name);

        addNodeSlot(node, "settings", profile->settings.get());
        addNodeSlot(node, "alter_settings", profile->alter_settings.get());
        addNodeSlot(node, "to_roles", profile->to_roles.get());

        return true;
    }
    else if (const auto * masking = dynamic_cast<const ASTCreateMaskingPolicyQuery *>(&ast))
    {
        if (masking->alter)
            node.add("alter", true);
        if (masking->attach)
            node.add("attach", true);
        if (masking->if_exists)
            node.add("if_exists", true);
        if (masking->if_not_exists)
            node.add("if_not_exists", true);
        if (masking->or_replace)
            node.add("or_replace", true);
        if (!masking->cluster.empty())
            node.add("cluster", masking->cluster);
        if (!masking->storage_name.empty())
            node.add("storage_name", masking->storage_name);

        if (!masking->name.empty())
            node.add("name", masking->name);
        if (!masking->database.empty())
            node.add("database", masking->database);
        if (!masking->table_name.empty())
            node.add("table", masking->table_name);
        if (!masking->new_name.empty())
            node.add("new_name", masking->new_name);

        if (masking->update_assignments)
            node.add("update_assignments", inlineExpressionList(masking->update_assignments));
        addNodeSlot(node, "where_condition", masking->where_condition);
        addNodeSlot(node, "roles", masking->roles.get());
        if (masking->priority != 0)
            node.add("priority", std::to_string(masking->priority));

        return true;
    }
    else if (const auto * drop_access = dynamic_cast<const ASTDropAccessEntityQuery *>(&ast))
    {
        node.add("entity_type", toString(drop_access->type));
        if (drop_access->if_exists)
            node.add("if_exists", true);
        if (!drop_access->cluster.empty())
            node.add("cluster", drop_access->cluster);
        if (!drop_access->storage_name.empty())
            node.add("storage_name", drop_access->storage_name);
        addStringList(node, "names", drop_access->names);
        addNodeSlot(node, "row_policy_names", drop_access->row_policy_names.get());
        /// DROP MASKING POLICY carries its target in a dedicated struct (the
        /// `short_name ON db.table` triple), not in `names`.
        if (drop_access->masking_policy_name)
        {
            auto masking_name = std::make_unique<JSONBuilder::JSONMap>();
            masking_name->add("short_name", drop_access->masking_policy_name->short_name);
            if (!drop_access->masking_policy_name->database.empty())
                masking_name->add("database", drop_access->masking_policy_name->database);
            masking_name->add("table", drop_access->masking_policy_name->table_name);
            node.add("masking_policy_name", std::move(masking_name));
        }

        return true;
    }
    else if (const auto * move_access = dynamic_cast<const ASTMoveAccessEntityQuery *>(&ast))
    {
        node.add("entity_type", toString(move_access->type));
        if (!move_access->cluster.empty())
            node.add("cluster", move_access->cluster);
        if (!move_access->storage_name.empty())
            node.add("storage_name", move_access->storage_name);
        addStringList(node, "names", move_access->names);
        addNodeSlot(node, "row_policy_names", move_access->row_policy_names.get());

        return true;
    }
    else if (const auto * execute_as = dynamic_cast<const ASTExecuteAsQuery *>(&ast))
    {
        addNodeSlot(node, "target_user", execute_as->target_user);
        addNodeSlot(node, "subquery", execute_as->subquery);

        return true;
    }
    else if (const auto * show_grants = dynamic_cast<const ASTShowGrantsQuery *>(&ast))
    {
        addNodeSlot(node, "for_roles", show_grants->for_roles.get());
        if (show_grants->with_implicit)
            node.add("with_implicit", true);
        if (show_grants->final)
            node.add("final", true);
        addNodeSlot(node, "settings", show_grants->settings_ast);
        addOutfileAndFormat(node, *show_grants);

        return true;
    }
    else if (const auto * show_create_access = dynamic_cast<const ASTShowCreateAccessEntityQuery *>(&ast))
    {
        node.add("entity_type", toString(show_create_access->type));
        addStringList(node, "names", show_create_access->names);
        addNodeSlot(node, "row_policy_names", show_create_access->row_policy_names.get());
        if (show_create_access->current_quota)
            node.add("current_quota", true);
        if (show_create_access->current_user)
            node.add("current_user", true);
        if (show_create_access->all)
            node.add("all", true);
        if (!show_create_access->short_name.empty())
            node.add("short_name", show_create_access->short_name);
        if (show_create_access->database_and_table_name)
        {
            node.add("database", show_create_access->database_and_table_name->first);
            node.add("table", show_create_access->database_and_table_name->second);
        }
        addNodeSlot(node, "settings", show_create_access->settings_ast);
        addOutfileAndFormat(node, *show_create_access);

        return true;
    }
    else if (const auto * show_access = dynamic_cast<const ASTShowAccessEntitiesQuery *>(&ast))
    {
        node.add("entity_type", toString(show_access->type));
        if (show_access->all)
            node.add("all", true);
        if (show_access->current_quota)
            node.add("current_quota", true);
        if (show_access->current_roles)
            node.add("current_roles", true);
        if (show_access->enabled_roles)
            node.add("enabled_roles", true);
        if (!show_access->short_name.empty())
            node.add("short_name", show_access->short_name);
        if (show_access->database_and_table_name)
        {
            node.add("database", show_access->database_and_table_name->first);
            node.add("table", show_access->database_and_table_name->second);
        }

        return true;
    }
    else if (const auto * roles_set = dynamic_cast<const ASTRolesOrUsersSet *>(&ast))
    {
        if (roles_set->all)
            node.add("all", true);
        if (roles_set->use_keyword_any)
            node.add("use_keyword_any", true);
        addStringList(node, "names", roles_set->names);
        if (roles_set->current_user)
            node.add("current_user", true);
        addStringList(node, "except_names", roles_set->except_names);
        if (roles_set->except_current_user)
            node.add("except_current_user", true);
        if (roles_set->id_mode)
            node.add("id_mode", true);

        return true;
    }
    else if (const auto * user_names = dynamic_cast<const ASTUserNamesWithHost *>(&ast))
    {
        auto users = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & child : user_names->children)
            users->add(formatASTAsJSON(*child));
        node.add("users", std::move(users));

        return true;
    }
    else if (const auto * user_name = dynamic_cast<const ASTUserNameWithHost *>(&ast))
    {
        /// toString() is "name" or "name@host"; split off the host so each part
        /// is exposed separately (host_pattern is "" / unset for the common case).
        String full = user_name->toString();
        String host_pattern = user_name->getHostPattern();
        if (!host_pattern.empty() && full.size() > host_pattern.size() + 1)
            node.add("name", full.substr(0, full.size() - host_pattern.size() - 1));
        else
            node.add("name", full);
        if (!host_pattern.empty())
            node.add("host_pattern", host_pattern);

        return true;
    }
    else if (const auto * auth = dynamic_cast<const ASTAuthenticationData *>(&ast))
    {
        if (auth->type)
            node.add("auth_type", toString(*auth->type));
        if (auth->contains_password)
            node.add("contains_password", true);
        if (auth->contains_hash)
            node.add("contains_hash", true);
        if (auth->ssl_cert_subject_type)
            node.add("ssl_cert_subject_type", *auth->ssl_cert_subject_type);
        /// valid_until is a separate member, not part of `children`.
        addNodeSlot(node, "valid_until", auth->valid_until);

        /// Password / hash / salt / server / realm literals (verbatim).
        auto args = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & child : auth->children)
            args->add(formatASTAsJSON(*child));
        node.add("arguments", std::move(args));

        return true;
    }
    else if (const auto * profile_elements = dynamic_cast<const ASTSettingsProfileElements *>(&ast))
    {
        auto elements = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & element : profile_elements->elements)
            elements->add(formatASTAsJSON(*element));
        node.add("elements", std::move(elements));

        return true;
    }
    else if (const auto * profile_element = dynamic_cast<const ASTSettingsProfileElement *>(&ast))
    {
        if (!profile_element->parent_profile.empty())
            node.add("parent_profile", profile_element->parent_profile);
        if (!profile_element->setting_name.empty())
            node.add("setting_name", profile_element->setting_name);
        if (profile_element->value)
            node.add("value", fieldToJSON(*profile_element->value));
        if (profile_element->min_value)
            node.add("min_value", fieldToJSON(*profile_element->min_value));
        if (profile_element->max_value)
            node.add("max_value", fieldToJSON(*profile_element->max_value));
        if (!profile_element->disallowed_values.empty())
        {
            auto disallowed = std::make_unique<JSONBuilder::JSONArray>();
            for (const auto & value : profile_element->disallowed_values)
                disallowed->add(fieldToJSON(value));
            node.add("disallowed_values", std::move(disallowed));
        }
        if (profile_element->writability)
            node.add("writability", String(settingWritabilityToString(*profile_element->writability)));
        if (profile_element->id_mode)
            node.add("id_mode", true);

        return true;
    }
    else if (const auto * alter_profile = dynamic_cast<const ASTAlterSettingsProfileElements *>(&ast))
    {
        addNodeSlot(node, "add_settings", alter_profile->add_settings.get());
        addNodeSlot(node, "modify_settings", alter_profile->modify_settings.get());
        addNodeSlot(node, "drop_settings", alter_profile->drop_settings.get());
        if (alter_profile->drop_all_settings)
            node.add("drop_all_settings", true);
        if (alter_profile->drop_all_profiles)
            node.add("drop_all_profiles", true);

        return true;
    }
    else if (const auto * policy_names = dynamic_cast<const ASTRowPolicyNames *>(&ast))
    {
        auto policies = std::make_unique<JSONBuilder::JSONArray>();
        for (const auto & full_name : policy_names->full_names)
        {
            auto item = std::make_unique<JSONBuilder::JSONMap>();
            item->add("short_name", full_name.short_name);
            if (!full_name.database.empty())
                item->add("database", full_name.database);
            if (!full_name.table_name.empty())
                item->add("table", full_name.table_name);
            policies->add(std::move(item));
        }
        node.add("policies", std::move(policies));

        return true;
    }
    else if (const auto * policy_name = dynamic_cast<const ASTRowPolicyName *>(&ast))
    {
        node.add("short_name", policy_name->full_name.short_name);
        if (!policy_name->full_name.database.empty())
            node.add("database", policy_name->full_name.database);
        if (!policy_name->full_name.table_name.empty())
            node.add("table", policy_name->full_name.table_name);

        return true;
    }
    else if (const auto * database_or_none = dynamic_cast<const ASTDatabaseOrNone *>(&ast))
    {
        if (database_or_none->none)
            node.add("none", true);
        else
            node.add("database", database_or_none->database_name);

        return true;
    }
    else if (const auto * ssh_key = dynamic_cast<const ASTPublicSSHKey *>(&ast))
    {
        node.add("key_type", ssh_key->type);
        node.add("key_base64", ssh_key->key_base64);

        return true;
    }
    else if (const auto * table_query = dynamic_cast<const ASTQueryWithTableAndOutput *>(&ast))
    {
        /// Generic fallback for the simple table-scoped statements that carry
        /// nothing but a target (EXISTS, SHOW CREATE, ...). Must stay after
        /// every richer ASTQueryWithTableAndOutput subclass above.
        addTableTarget(node, *table_query);

        return true;
    }

    return false;
}

/// The node's "type" id.
///
/// By default it is `IAST::getID(' ')` trimmed at the first space — getID packs
/// auxiliary data after a space delimiter (e.g. "Function quantile"), and the
/// structured info is re-exposed via enrichNode. A few classes, however, return
/// a purely descriptive getID that itself contains a space ("Dictionary
/// lifetime", ...). Trimming those collapses the five Dictionary* classes onto
/// the same "Dictionary" — so they are spelled out explicitly here.
String astTypeName(const IAST & ast)
{
    if (dynamic_cast<const ASTDictionaryLifetime *>(&ast))
        return "DictionaryLifetime";
    if (dynamic_cast<const ASTDictionaryLayout *>(&ast))
        return "DictionaryLayout";
    if (dynamic_cast<const ASTDictionaryRange *>(&ast))
        return "DictionaryRange";
    if (dynamic_cast<const ASTDictionarySettings *>(&ast))
        return "DictionarySettings";

    /// getID is "Set"; rename to avoid confusion with SET statements / the Set
    /// data structure — this is the SETTINGS clause.
    if (dynamic_cast<const ASTSetQuery *>(&ast))
        return "Settings";

    /// ASTShowIndexesQuery::getID is a mis-set "ShowColumns" (shared verbatim
    /// with ASTShowColumnsQuery), which would make the two distinct statements
    /// SHOW COLUMNS and SHOW INDEXES indistinguishable. Spell it out so the
    /// type alone identifies the statement.
    if (dynamic_cast<const ASTShowIndexesQuery *>(&ast))
        return "ShowIndexes";

    /// GRANT and REVOKE share one class (ASTGrantQuery); split by is_revoke so
    /// the type alone identifies the statement.
    if (const auto * grant = dynamic_cast<const ASTGrantQuery *>(&ast))
        return grant->is_revoke ? "RevokeQuery" : "GrantQuery";

    /// These access statements have descriptive getID()s that contain spaces
    /// (e.g. "CREATE ROW POLICY or ALTER ROW POLICY query", "DROP USER query"),
    /// which would otherwise trim to "CREATE" / "DROP" / "SHOW" / "MOVE".
    if (dynamic_cast<const ASTCreateRowPolicyQuery *>(&ast))
        return "CreateRowPolicyQuery";
    if (dynamic_cast<const ASTCreateMaskingPolicyQuery *>(&ast))
        return "CreateMaskingPolicyQuery";
    if (dynamic_cast<const ASTDropAccessEntityQuery *>(&ast))
        return "DropAccessEntityQuery";
    if (dynamic_cast<const ASTMoveAccessEntityQuery *>(&ast))
        return "MoveAccessEntityQuery";
    if (dynamic_cast<const ASTShowCreateAccessEntityQuery *>(&ast))
        return "ShowCreateAccessEntityQuery";
    if (dynamic_cast<const ASTShowAccessEntitiesQuery *>(&ast))
        return "ShowAccessEntitiesQuery";

    /// getID is the raw class name "ASTTransactionControl"; drop the AST prefix
    /// so it matches the rest of the type ids (BEGIN/COMMIT/ROLLBACK/SET SNAPSHOT
    /// are split out into the `action` field).
    if (dynamic_cast<const ASTTransactionControl *>(&ast))
        return "TransactionControl";

    /// getID is the descriptive "Refresh strategy definition", which would
    /// otherwise trim to "Refresh".
    if (dynamic_cast<const ASTRefreshStrategy *>(&ast))
        return "RefreshStrategy";

    /// getID is the raw class name "ASTObjectTypeArgument"; drop the AST prefix.
    if (dynamic_cast<const ASTObjectTypeArgument *>(&ast))
        return "ObjectTypeArgument";

    String id = ast.getID(' ');
    auto space_pos = id.find(' ');
    if (space_pos != String::npos)
        id.resize(space_pos);
    return id;
}

}

JSONBuilder::ItemPtr formatASTAsJSON(const IAST & ast)
{
    auto node = std::make_unique<JSONBuilder::JSONMap>();

    node->add("type", astTypeName(ast));

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

JSONBuilder::ItemPtr formatASTAsJSONDocument(const IAST & ast)
{
    auto document = std::make_unique<JSONBuilder::JSONMap>();
    document->add("version", AST_JSON_FORMAT_VERSION);
    document->add("ast", formatASTAsJSON(ast));
    return document;
}

}
