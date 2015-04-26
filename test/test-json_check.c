#include <string.h>

#include "test.h"
#include "../src/json_check.h"

static void json_check_1(){
	const char input[] = "{\"records\":5,\"free\":0,\"tablecache\":23,\"datacache\":25,\"datacache_density\":75,\"uptime\":\"19 days, 23:21:05\",\"client_requests\":206,\"description\":\"Database statistics\",\"status\":\"COMMAND_OK\",\"success\":1}";
	ASSERT(json_valid(input));
}

static void json_check_2(){
	const char input[] = "{\"widget\": {\n    \"debug\": \"on\",\n    \"window\": {\n        \"title\": \"Sample Konfabulator Widget\",\n        \"name\": \"main_window\",\n        \"width\": 500,\n        \"height\": 500\n    },\n    \"image\": { \n        \"src\": \"Images/Sun.png\",\n        \"name\": \"sun1\",\n        \"hOffset\": 250,\n        \"vOffset\": 250,\n        \"alignment\": \"center\"\n    },\n    \"text\": {\n        \"data\": \"Click Here\",\n        \"size\": 36,\n        \"style\": \"bold\",\n        \"name\": \"text1\",\n        \"hOffset\": 250,\n        \"vOffset\": 100,\n        \"alignment\": \"center\",\n        \"onMouseUp\": \"sun1.opacity = (sun1.opacity / 100) * 90;\"\n    }\n}} ";
	ASSERT(json_valid(input));
}

static void json_check_3(){
	const char input[] = "{\"web-app\": {\n  \"servlet\": [   \n    {\n      \"servlet-name\": \"cofaxCDS\",\n      \"servlet-class\": \"org.cofax.cds.CDSServlet\",\n      \"init-param\": {\n        \"configGlossary:installationAt\": \"Philadelphia, PA\",\n        \"configGlossary:adminEmail\": \"ksm@pobox.com\",\n        \"configGlossary:poweredBy\": \"Cofax\",\n        \"configGlossary:poweredByIcon\": \"/images/cofax.gif\",\n        \"configGlossary:staticPath\": \"/content/static\",\n        \"templateProcessorClass\": \"org.cofax.WysiwygTemplate\",\n        \"templateLoaderClass\": \"org.cofax.FilesTemplateLoader\",\n        \"templatePath\": \"templates\",\n        \"templateOverridePath\": \"\",\n        \"defaultListTemplate\": \"listTemplate.htm\",\n        \"defaultFileTemplate\": \"articleTemplate.htm\",\n        \"useJSP\": false,\n        \"jspListTemplate\": \"listTemplate.jsp\",\n        \"jspFileTemplate\": \"articleTemplate.jsp\",\n        \"cachePackageTagsTrack\": 200,\n        \"cachePackageTagsStore\": 200,\n        \"cachePackageTagsRefresh\": 60,\n        \"cacheTemplatesTrack\": 100,\n        \"cacheTemplatesStore\": 50,\n        \"cacheTemplatesRefresh\": 15,\n        \"cachePagesTrack\": 200,\n        \"cachePagesStore\": 100,\n        \"cachePagesRefresh\": 10,\n        \"cachePagesDirtyRead\": 10,\n        \"searchEngineListTemplate\": \"forSearchEnginesList.htm\",\n        \"searchEngineFileTemplate\": \"forSearchEngines.htm\",\n        \"searchEngineRobotsDb\": \"WEB-INF/robots.db\",\n        \"useDataStore\": true,\n        \"dataStoreClass\": \"org.cofax.SqlDataStore\",\n        \"redirectionClass\": \"org.cofax.SqlRedirection\",\n        \"dataStoreName\": \"cofax\",\n        \"dataStoreDriver\": \"com.microsoft.jdbc.sqlserver.SQLServerDriver\",\n        \"dataStoreUrl\": \"jdbc:microsoft:sqlserver://LOCALHOST:1433;DatabaseName=goon\",\n        \"dataStoreUser\": \"sa\",\n        \"dataStorePassword\": \"dataStoreTestQuery\",\n        \"dataStoreTestQuery\": \"SET NOCOUNT ON;select test='test';\",\n        \"dataStoreLogFile\": \"/usr/local/tomcat/logs/datastore.log\",\n        \"dataStoreInitConns\": 10,\n        \"dataStoreMaxConns\": 100,\n        \"dataStoreConnUsageLimit\": 100,\n        \"dataStoreLogLevel\": \"debug\",\n        \"maxUrlLength\": 500}},\n    {\n      \"servlet-name\": \"cofaxEmail\",\n      \"servlet-class\": \"org.cofax.cds.EmailServlet\",\n      \"init-param\": {\n      \"mailHost\": \"mail1\",\n      \"mailHostOverride\": \"mail2\"}},\n    {\n      \"servlet-name\": \"cofaxAdmin\",\n      \"servlet-class\": \"org.cofax.cds.AdminServlet\"},\n \n    {\n      \"servlet-name\": \"fileServlet\",\n      \"servlet-class\": \"org.cofax.cds.FileServlet\"},\n    {\n      \"servlet-name\": \"cofaxTools\",\n      \"servlet-class\": \"org.cofax.cms.CofaxToolsServlet\",\n      \"init-param\": {\n        \"templatePath\": \"toolstemplates/\",\n        \"log\": 1,\n        \"logLocation\": \"/usr/local/tomcat/logs/CofaxTools.log\",\n        \"logMaxSize\": \"\",\n        \"dataLog\": 1,\n        \"dataLogLocation\": \"/usr/local/tomcat/logs/dataLog.log\",\n        \"dataLogMaxSize\": \"\",\n        \"removePageCache\": \"/content/admin/remove?cache=pages&id=\",\n        \"removeTemplateCache\": \"/content/admin/remove?cache=templates&id=\",\n        \"fileTransferFolder\": \"/usr/local/tomcat/webapps/content/fileTransferFolder\",\n        \"lookInContext\": 1,\n        \"adminGroupID\": 4,\n        \"betaServer\": true}}],\n  \"servlet-mapping\": {\n    \"cofaxCDS\": \"/\",\n    \"cofaxEmail\": \"/cofaxutil/aemail/*\",\n    \"cofaxAdmin\": \"/admin/*\",\n    \"fileServlet\": \"/static/*\",\n    \"cofaxTools\": \"/tools/*\"},\n \n  \"taglib\": {\n    \"taglib-uri\": \"cofax.tld\",\n    \"taglib-location\": \"/WEB-INF/tlds/cofax.tld\"}}}";
	ASSERT(json_valid(input));
}

static void json_check_4(){
	const char input[] = "{\n    \"glossary\": {\n        \"title\": \"example glossary\",\n\t\t\"GlossDiv\": {\n            \"title\": \"S\",\n\t\t:\t\"GlossList\": {\n                \"GlossEntry\": {\n                    \"ID\": \"SGML\",\n\t\t\t\t\t\"SortAs\": \"SGML\",\n\t\t\t\t\t\"GlossTerm\": \"Standard Generalized Markup Language\",\n\t\t\t\t\t\"Acronym\": \"SGML\",\n\t\t\t\t\t\"Abbrev\": \"ISO 8879:1986\",\n\t\t\t\t\t\"GlossDef\": {\n                        \"para\": \"A meta-markup language, used to create markup languages such as DocBook.\",\n\t\t\t\t\t\t\"GlossSeeAlso\": [\"GML\", \"XML\"]\n                    },\n\t\t\t\t\t\"GlossSee\": \"markup\"\n                }\n            }\n        }\n    }\n}";
	ASSERT(!json_valid(input));
}

TEST_IMPL(json_check) {

	TESTCASE("json_check");

	/* Run testcase */
	json_check_1();
	json_check_2();
	json_check_3();
	json_check_4();

	RETURN_OK();
}
