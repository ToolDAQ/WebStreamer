#include "Factory.h"
#include "Unity.h"

Tool* Factory(std::string tool) {
Tool* ret=0;

// if (tool=="Type") tool=new Type;
if (tool=="DummyTool") ret=new DummyTool;
if (tool=="JobManager") ret=new JobManager;
if (tool=="WebSocket") ret=new WebSocket;
  if (tool=="FakeDataSource") ret=new FakeDataSource;
return ret;
}
