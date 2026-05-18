#include "Factory.h"
#include "Unity.h"

Tool* Factory(std::string tool) {
Tool* ret=0;

// if (tool=="Type") tool=new Type;
if (tool=="DummyTool") ret=new DummyTool;
if (tool=="JobManager") ret=new JobManager;
if (tool=="WebSocket") ret=new WebSocket;
if (tool=="FakeDataSource") ret=new FakeDataSource;
  if (tool=="HitmapFakeData") ret=new HitmapFakeData;
  if (tool=="TriggerRateFakeData") ret=new TriggerRateFakeData;
  if (tool=="EventDisplayFakeData") ret=new EventDisplayFakeData;
return ret;
}
