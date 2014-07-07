${remoteClass.name}ImplInternal.cpp
#include <gst/gst.h>
<#list remoteClassDependencies(remoteClass) as dependency>
<#if model.remoteClasses?seq_contains(dependency)>
#include "${dependency.name}Impl.hpp"
<#else>
#include "${dependency.name}.hpp"
</#if>
</#list>
#include "${remoteClass.name}Impl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>

namespace kurento
{
<#if (!remoteClass.abstract) && (remoteClass.constructors[0])??>

MediaObjectImpl *${remoteClass.name}Impl::Factory::createObjectPointer (const Json::Value &params) const
{
  <#list remoteClass.constructors[0].params as param>
  <#if model.remoteClasses?seq_contains(param.type.type)><#lt>
  ${getCppObjectType(param.type, false, "", "Impl")} ${param.name}<#rt>
  <#else><#lt>
  ${getCppObjectType(param.type, false)} ${param.name}<#rt>
  </#if>
    <#lt><#if param.type.name = "int"> = 0<#rt>
    <#lt><#elseif param.type.name = "boolean"> = false<#rt>
    <#lt><#elseif param.type.name = "float"> = 0.0<#rt>
    <#lt></#if>;
  </#list>

  <#list remoteClass.constructors[0].params as param>
  if (!params.isMember ("${param.name}") ) {
    <#if (param.defaultValue)??>
    /* param '${param.name}' not present, using default */
    <#if param.type.name = "String" || param.type.name = "int" || param.type.name = "boolean">
    ${param.name} = ${param.defaultValue};
    <#elseif model.complexTypes?seq_contains(param.type.type) >
      <#if param.type.type.typeFormat == "REGISTER">
    // TODO, deserialize default param value for type '${param.type}'
      <#elseif param.type.type.typeFormat == "ENUM">
    ${param.name} = std::shared_ptr<${param.type.name}> (new ${param.type.name} (${param.defaultValue}) );
      <#else>
    // TODO, deserialize default param value for type '${param.type}' not expected
      </#if>
    <#else>
    // TODO, deserialize default param value for type '${param.type}'
    </#if>
    <#else>
    <#if (param.optional)>
    // Warning, optional constructor parameter '${param.name}' but no default value provided
    <#else>
    /* param '${param.name}' not present, raise exception */
    JsonRpc::CallException e (JsonRpc::ErrorCode::SERVER_ERROR_INIT,
                              "'${param.name}' parameter is requiered");
    throw e;
    </#if>
    </#if>
  } else {
    JsonSerializer s (false);
    s.JsonValue = params;
    s.SerializeNVP (${param.name});
  }

  </#list>
  return createObject (<#rt>
     <#lt><#list remoteClass.constructors[0].params as param><#rt>
        <#lt>${param.name}<#rt>
        <#lt><#if param_has_next>, </#if><#rt>
     <#lt></#list>);
}
</#if>

void
${remoteClass.name}Impl::invoke (std::shared_ptr<MediaObjectImpl> obj, const std::string &methodName, const Json::Value &params, Json::Value &response)
{
<#list remoteClass.methods as method><#rt>
  if (methodName == "${method.name}" && params.size() == ${method.params?size}) {
    Json::Value aux;
    <#if method.return??>
    ${getCppObjectType(method.return.type, false)} ret;
    JsonSerializer serializer (true);
    </#if>
    <#list method.params as param>
    ${getCppObjectType(param.type, false)} ${param.name};
    </#list>

    <#list method.params as param>
    <#assign json_method = "">
    <#assign json_value_type = "">
    <#assign type_description = "">
    if (!params.isMember ("${param.name}") ) {
      <#if (param.defaultValue)??>
      /* param '${param.name}' not present, using default */
      <#if param.type.name = "String" || param.type.name = "int" || param.type.name = "boolean">
      ${param.name} = ${param.defaultValue};
      <#elseif model.complexTypes?seq_contains(param.type.type) >
        <#if param.type.type.typeFormat == "REGISTER">
      // TODO, deserialize default param value for type '${param.type}'
        <#elseif param.type.type.typeFormat == "ENUM">
      ${param.name} = std::shared_ptr<${param.type.name}> (new ${param.type.name} (${param.defaultValue}) );
        <#else>
      // TODO, deserialize default param value for type '${param.type}' not expected
        </#if>
      <#else>
      // TODO, deserialize default param value for type '${param.type}'
      </#if>
      <#else>
      <#if (param.optional)>
      // Warning, optional constructor parameter '${param.name}' but no default value provided
      </#if>
      /* param '${param.name}' not present, raise exception */
      JsonRpc::CallException e (JsonRpc::ErrorCode::SERVER_ERROR_INIT,
                                "'${param.name}' parameter is requiered");
      throw e;
      </#if>
    } else {
      <#if model.remoteClasses?seq_contains(param.type.type) >
      std::shared_ptr<MediaObject> obj;

      </#if>
      aux = params["${param.name}"];
      <#if param.type.isList()>
        <#assign json_method = "List">
        <#assign json_value_type = "arrayValue">
        <#assign type_description = "list">
      <#elseif param.type.name = "String">
        <#assign json_method = "String">
        <#assign json_value_type = "stringValue">
        <#assign type_description = "string">
      <#elseif param.type.name = "int">
        <#assign json_method = "Int">
        <#assign json_value_type = "intValue">
        <#assign type_description = "integer">
      <#elseif param.type.name = "boolean">
        <#assign json_method = "Bool">
        <#assign json_value_type = "booleanValue">
        <#assign type_description = "boolean">
      <#elseif param.type.name = "double" || param.type.name = "float">
        <#assign json_method = "Double">
        <#assign json_value_type = "realValue">
        <#assign type_description = "double">
      <#elseif model.complexTypes?seq_contains(param.type.type) >
        <#if param.type.type.typeFormat == "ENUM">
          <#assign json_method = "String">
          <#assign json_value_type = "stringValue">
          <#assign type_description = "string">
        <#else>
          <#assign json_method = "Object">
          <#assign json_value_type = "objectValue">
          <#assign type_description = "object">
        </#if>
      <#elseif model.remoteClasses?seq_contains(param.type.type) >
        <#assign json_method = "String">
        <#assign json_value_type = "stringValue">
        <#assign type_description = "string">
      </#if>
      <#if json_method != "" && type_description != "" && json_value_type != "">

      if (!aux.isConvertibleTo (Json::ValueType::${json_value_type}) ) {
        /* param '${param.name}' has invalid type value, raise exception */
        JsonRpc::CallException e (JsonRpc::ErrorCode::SERVER_ERROR_INIT,
                                  "'${param.name}' parameter should be a ${type_description}");
        throw e;
      }

        <#if model.complexTypes?seq_contains(param.type.type) >
          <#if param.type.type.typeFormat == "REGISTER">
      ${param.name} = std::shared_ptr<${param.type.name}> (new ${param.type.name} (aux) );
          <#elseif param.type.type.typeFormat == "ENUM">
      ${param.name} = std::shared_ptr<${param.type.name}> (new ${param.type.name} (aux.as${json_method} () ) );
          <#else>
      // TODO, deserialize param value for type '${param.type}' not expected
          </#if>
        <#elseif model.remoteClasses?seq_contains(param.type.type) >
      try {
        obj = ${param.type.name}Impl::Factory::getObject (aux.as${json_method} () );
      } catch (JsonRpc::CallException &ex) {
        JsonRpc::CallException e (JsonRpc::ErrorCode::SERVER_ERROR_INIT,
                                  "'${param.name}' object not found: " + ex.getMessage() );
        throw e;
      }

      ${param.name} = std::dynamic_pointer_cast<${param.type.name}> (obj);

      if (!${param.name}) {
        JsonRpc::CallException e (JsonRpc::ErrorCode::SERVER_ERROR_INIT,
                                  "'${param.name}' object has a invalid type");
        throw e;
      }
        <#else>
      ${param.name} = aux.as${json_method} ();
        </#if>
      <#else>
      // TODO, deserialize param type '${param.type}'
      </#if>
    }

    </#list>
    // TODO: Implement method ${method.name}
    std::shared_ptr<${remoteClass.name}Impl> finalObj;
    finalObj = std::dynamic_pointer_cast<${remoteClass.name}Impl> (obj);

    if (!finalObj) {
      JsonRpc::CallException e (JsonRpc::ErrorCode::SERVER_ERROR_INIT,
                                "Object not found or has incorrect type");
      throw e;
    }

    <#if method.return??>
    ret = <#rt>
    <#else><#rt>
    </#if>finalObj->${method.name} (<#list method.params as param>${param.name}<#if param_has_next>, </#if></#list>);
    <#if method.return??>
    serializer.SerializeNVP (ret);
    response = serializer.JsonValue["ret"];
    </#if>
    return;
  }

</#list>
<#if (remoteClass.extends)??>
  ${remoteClass.extends.name}Impl::invoke (obj, methodName, params, response);
<#else>
  JsonRpc::CallException e (JsonRpc::ErrorCode::SERVER_ERROR_INIT,
                            "Method '" + methodName + "' with " + std::to_string (params.size() ) + " parameters not found");
  throw e;
</#if>
}

bool
${remoteClass.name}Impl::connect (const std::string &eventType, std::shared_ptr<EventHandler> handler)
{
<#list remoteClass.events as event>

  if ("${event.name}" == eventType) {
    sigc::connection conn = signal${event.name}.connect ([ &, handler] (${event.name} event) {
      JsonSerializer s (true);

      s.Serialize ("data", event);
      s.Serialize ("object", this);
      s.JsonValue["type"] = "${event.name}";
      handler->sendEvent (s.JsonValue);
    });
    handler->setConnection (conn);
    return true;
  }
</#list>

<#if (remoteClass.extends)??>
  return ${remoteClass.extends.name}Impl::connect (eventType, handler);
<#else>
  return false;
</#if>
}

void
Serialize (std::shared_ptr<kurento::${remoteClass.name}Impl> &object, JsonSerializer &serializer)
{
  if (serializer.IsWriter) {
    if (object) {
      object->Serialize (serializer);
    }
  } else {
    try {
      std::shared_ptr<kurento::MediaObjectImpl> aux;
      aux = kurento::${remoteClass.name}Impl::Factory::getObject (serializer.JsonValue.asString () );
      object = std::dynamic_pointer_cast<kurento::${remoteClass.name}Impl> (aux);
      return;
    } catch (KurentoException &ex) {
      throw KurentoException (MARSHALL_ERROR,
                              "'${remoteClass.name}Impl' object not found: " + ex.getMessage() );
    }
  }
}

void
${remoteClass.name}Impl::Serialize (JsonSerializer &serializer)
{
  if (serializer.IsWriter) {
    try {
      Json::Value v (getId() );

      serializer.JsonValue = v;
    } catch (std::bad_cast &e) {
    }
  } else {
    try {
      std::shared_ptr<kurento::MediaObjectImpl> aux;
      aux = kurento::${remoteClass.name}Impl::Factory::getObject (serializer.JsonValue.asString () );
      *this = *std::dynamic_pointer_cast<kurento::${remoteClass.name}Impl> (aux).get();
      return;
    } catch (KurentoException &ex) {
      throw KurentoException (MARSHALL_ERROR,
                              "'${remoteClass.name}Impl' object not found: " + ex.getMessage() );
    }
  }
}

void
Serialize (std::shared_ptr<kurento::${remoteClass.name}> &object, JsonSerializer &serializer)
{
  std::shared_ptr<kurento::${remoteClass.name}Impl> aux = std::dynamic_pointer_cast<kurento::${remoteClass.name}Impl> (object);

  Serialize (aux, serializer);
}

} /* kurento */
