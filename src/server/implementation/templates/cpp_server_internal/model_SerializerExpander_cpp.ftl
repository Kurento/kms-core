SerializerExpander.cpp
/* Generated using ktool-rom-processor */

<#list model.remoteClasses as remoteClass>
#include "${remoteClass.name}Impl.hpp"
</#list>
<#list model.events as event>
#include "${event.name}.hpp"
</#list>
<#list model.complexTypes as complexType>
#include "${complexType.name}.hpp"
</#list>

#include <jsonrpc/JsonSerializer.hpp>

namespace kurento
{

void dummy_${.now?long?c} ()
{
<#list model.remoteClasses as remoteClass>
  {
    JsonSerializer s (true);
    std::shared_ptr<${remoteClass.name}> object;

    s.SerializeNVP (object);
  }
</#list>
<#list model.events as event>
  {
    JsonSerializer s (true);
    std::shared_ptr<${event.name}> object;

    s.SerializeNVP (object);
  }
</#list>
<#list model.complexTypes as complexType>
  {
    JsonSerializer s (true);
    std::shared_ptr<${complexType.name}> object;

    s.SerializeNVP (object);
  }
</#list>
}

} /* kurento */
