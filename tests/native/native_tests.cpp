#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include "DeliveryPolicy.h"
#include "Roster.h"
#include "UiView.h"

int passed = 0;
void check(bool value, const char *name) {
    if (!value) throw std::runtime_error(name);
    ++passed;
    std::cout << "PASS " << name << '\n';
}
void testPolicy() {
    using door::Outcome;
    auto result = door::classifyReply;
    check(result(202,true,"queued","") == Outcome::Accepted, "202 queued accepted");
    check(result(200,true,"playing","") == Outcome::Accepted, "duplicate playing accepted");
    check(result(200,true,"done","") == Outcome::Accepted, "done accepted");
    check(result(200,true,"failed","") == Outcome::PlaybackFailed, "playback failure never success");
    check(result(200,false,"queued","") == Outcome::Unknown, "mismatched request or boot locked");
    check(result(200,true,"mystery","") == Outcome::Unknown, "unknown state never success");
    check(result(-1,false,"","") == Outcome::Unknown, "timeout locked");
    check(result(-2,false,"","") == Outcome::Unknown, "malformed JSON locked");
    check(result(503,false,"","audio_not_ready") == Outcome::Unknown, "server error locked");
    check(result(408,false,"","") == Outcome::Unknown, "HTTP timeout locked");
    check(result(409,false,"","audio_restarted") == Outcome::Unknown, "Audio restart locked");
    check(result(409,false,"","request_conflict") == Outcome::Unknown, "conflicting request locked");
    check(result(429,false,"","queue_full") == Outcome::Rejected, "known full queue refusal");
    check(result(403,false,"","unauthorized") == Outcome::Rejected, "known key refusal");
    check(result(404,false,"","id_not_found") == Outcome::Rejected, "known missing ID refusal");
    check(result(404,false,"","not_found") == Outcome::Unknown, "wrong HTTP path not trusted");
    check(door::retryWindowOpen(540000,0), "retry at nine-minute boundary");
    check(!door::retryWindowOpen(540001,0), "retry after nine minutes blocked");
    check(door::retryWindowOpen(10,UINT32_MAX-20), "retry clock wrap-safe");
}
void testRoster() {
    fs::FS fs;
    String error;
    Person student, teachers[15];
    size_t count = 0;
    const std::string header = "studentId,class,maskedName\n";
    fs.files["/students.csv"] = std::string("\xef\xbb\xbf") + header + "00001,2A,陳○文\n";
    check(studentsReady(fs,error), "student BOM accepted");
    check(findStudent(fs,"00001",student,error) && student.id == "00001" &&
          student.maskedName == "陳○文", "exact student match with leading zeros");
    check(!findStudent(fs,"1",student,error) && error == "invalid_id", "leading zeros not discarded");
    check(!findStudent(fs,"test1",student,error) && error == "invalid_id", "lowercase rejected");
    fs.files["/students.csv"] += "00001,2B,林○明\n";
    check(!findStudent(fs,"00001",student,error) && error == "duplicate_student", "duplicate match rejected");
    fs.files["/students.csv"] = header + "00001,2A,陳○文\nBROKEN,\"unterminated";
    check(!findStudent(fs,"00001",student,error) && error == "students_invalid", "malformed suffix cannot hide after match");
    fs.files["/students.csv"] = "studentId,class,displayName\n00001,2A,陳大文\n";
    check(!studentsReady(fs,error), "full-name header rejected");
    fs.files["/students.csv"] = header + "00001,\"2,A\",陳○文\n";
    check(findStudent(fs,"00001",student,error) && student.className == "2,A", "quoted comma parsed");
    fs.files["/teachers.csv"] = "teacherId,displayName\n01,李老師\n02,王老師\n";
    check(loadTeachers(fs,teachers,count,error) && count == 2 && teachers[0].id == "01", "teacher roster loaded");
    fs.files["/teachers.csv"] += "01,陳老師\n";
    check(!loadTeachers(fs,teachers,count,error) && error == "duplicate_teacher", "duplicate teacher ID rejected");
    fs.files["/teachers.csv"] = "teacherId,displayName\n";
    for (int i=0;i<16;++i) fs.files["/teachers.csv"] += std::to_string(i) + ",老師\n";
    check(!loadTeachers(fs,teachers,count,error), "more than 15 teachers rejected");
    fs.files["/students.csv"]=header+"E12345,2A,陳○文\n";
    check(findStudent(fs,"12345",student,error),"E plus five alias accepts digits only");
    fs.files["/students.csv"] += "12345,2B,林○明\n";
    check(!findStudent(fs,"12345",student,error)&&error=="duplicate_student","E alias collision rejected");
    fs.files.erase("/students.csv");
    check(!studentsReady(fs,error), "missing student file rejected");
}
void verifyHits(const UiView &view, const char *name) {
    bool good = true;
    for (int i=0;i<view.hitCount;++i) {
        const Hit &a = view.hits[i];
        good &= a.x >= 0 && a.y >= 0 && a.x+a.w <= 1024 && a.y+a.h <= 600;
        good &= view.hitTest(a.x+a.w/2,a.y+a.h/2) == &view.hits[i];
        for (int j=i+1;j<view.hitCount;++j) {
            const Hit &b = view.hits[j];
            bool overlap = a.x < b.x+b.w && b.x < a.x+a.w && a.y < b.y+b.h && b.y < a.y+a.h;
            good &= !overlap;
        }
    }
    check(good,name);
}
void ppm(const std::filesystem::path &path, const std::vector<uint16_t> &pixels) {
    std::ofstream file(path,std::ios::binary);
    file << "P6\n1024 600\n255\n";
    for (uint16_t value : pixels) {
        unsigned char rgb[3] = {static_cast<unsigned char>(((value>>11)&31)*255/31),
                               static_cast<unsigned char>(((value>>5)&63)*255/63),
                               static_cast<unsigned char>((value&31)*255/31)};
        file.write(reinterpret_cast<char*>(rgb),3);
    }
}
void testNetworkRules(){
    check(staffroom::studentId5("00001"),"five digits preserve leading zeros");
    for(auto bad:{"000001","1234","E12345","１２３４５","12A45","12345 "})check(!staffroom::studentId5(bad),"non-five-digit UI ID rejected");
    for(auto good:{"192.168.1.21","10.0.0.5","172.16.0.1"})check(staffroom::unicastIPv4(good),"unicast IPv4 accepted");
    for(auto bad:{"","localhost","192.168.01.2","192.168.1.256","192.168.1.2.3","127.0.0.1","224.0.0.1","169.254.1.2","1.2.3"})check(!staffroom::unicastIPv4(bad),"malformed/reserved IPv4 rejected");
    check(staffroom::validNetworkFields("wifi","",0,"","",false,false),"open Wi-Fi supported");
    check(!staffroom::validNetworkFields("wifi","short",0,"","",false,false),"short WPA password blocked");
    check(staffroom::validNetworkFields("wifi","password",0,"","",false,false),"WPA password accepted");
    check(!staffroom::validNetworkFields("wifi","password",1,"teacher","",false,false),"enterprise fails closed without CA");
    check(staffroom::validNetworkFields("wifi","password",1,"teacher","",true,false),"enterprise accepts installed CA");
    check(staffroom::validNetworkFields("wifi","password",2,"teacher","",false,true),"explicit insecure EAP opt-in works");
    check(!staffroom::validNetworkFields("wifi","password",1,"","",true,false),"enterprise username required");
}
void testView(const std::filesystem::path &output) {
    std::filesystem::create_directories(output);
    std::vector<uint16_t> pixels(1024*600);
    UiView view;
    view.begin(pixels.data());
    ViewState s;
    s.ready = true; s.demo = true;
    view.render(s);
    check(view.hitCount == 15, "ten numeric keys, clear/delete/next, network and welcome");
    verifyHits(view,"input hitboxes in bounds and disjoint");
    ppm(output/"01-demo-input.ppm",pixels);
    s.studentId = "00001"; s.studentClass="2A"; s.maskedName="陳○文";
    s.page = UiPage::Teachers;
    s.teacherCount = 15;
    for (size_t i=0;i<15;++i) s.teacherNames[i] = "測試老師" + String(i+1);
    s.teacherNames[0] = "李老師"; s.teacherNames[1]="王老師"; s.teacherNames[2]="陳老師";
    view.render(s);
    check(view.hitCount == 16,"15 teachers plus back button");
    verifyHits(view,"teacher hitboxes in bounds and disjoint");
    ppm(output/"02-teachers-15.ppm",pixels);
    s.page=UiPage::Confirm; s.selectedTeacher="李老師";
    view.render(s);
    verifyHits(view,"confirmation hitboxes disjoint");
    ppm(output/"03-demo-confirm.ppm",pixels);
    s.page=UiPage::Complete;
    view.render(s);
    check(view.hitCount==1,"demo completion next-student only");
    ppm(output/"04-demo-complete.ppm",pixels);
    s.demo=false; s.wifi=true; s.completeState="queued";
    view.render(s);
    ppm(output/"05-live-queued.ppm",pixels);
    s.page=UiPage::Confirm; s.frozen=true; s.notice="尚未確認，請查詢原請求或找老師";
    view.render(s);
    check(view.hitCount==2 && view.hits[0].action==UiAction::Query &&
          view.hits[1].action==UiAction::Submit,"uncertain request has query and same-request retry only");
    verifyHits(view,"uncertain request hitboxes disjoint");
    ppm(output/"06-live-unknown.ppm",pixels);
    s.canRetry=false; s.notice="已超過安全重試時限，請找老師確認";
    view.render(s);
    check(view.hitCount==1 && view.hits[0].action==UiAction::Query,"expired request cannot retry");
    ppm(output/"07-live-expired.ppm",pixels);
    s.ready=false; s.frozen=false; s.wifi=false; s.blockingTitle="正式模式設定未完成";
    s.blockingHelp="請修改 config.h，或先切換演示模式";
    s.notice="檢查 Wi-Fi、密鑰、SYSTEM_ID 與 AUDIO_HOST";
    view.render(s);
    s.page=UiPage::Input;s.studentId="";view.render(s);
    check(view.hitCount==1 && view.hits[0].action==UiAction::OpenNetwork,"configuration error permits network recovery only");
    ppm(output/"08-config-required.ppm",pixels);
    s.ready=true;s.demo=false;s.studentId="";s.ownIp="192.168.1.20";s.ownMac="AA:BB:CC:DD:EE:FF";
    s.audioHost="192.168.1.21";s.matrixHost="192.168.1.22";s.networkSsid="Staff WiFi";
    s.notice="";s.wifi=true;s.netModeName="普通 / 開放";s.netStatus="輸入 Audio IP 或自動尋找";
    s.page=UiPage::NetUnlock;s.netPin="123456";view.render(s);verifyHits(view,"PIN keyboard disjoint");ppm(output/"09-network-pin.ppm",pixels);
    s.page=UiPage::Network;view.render(s);verifyHits(view,"network status controls disjoint");ppm(output/"10-network-status.ppm",pixels);
    s.netCredentials=true;s.netFields[0]="Staff WiFi";s.netFields[1]="********";view.render(s);verifyHits(view,"Wi-Fi form disjoint");ppm(output/"11-network-wifi.ppm",pixels);
    s.page=UiPage::NetworkEdit;s.netEditCaption="Wi-Fi 密碼";s.netSecret=true;s.netEditValue="private-test";
    for(int i=0;i<3;i++){s.netKeyboard=i;view.render(s);verifyHits(view,"network editor keys disjoint");ppm(output/("12-editor-"+std::to_string(i)+".ppm"),pixels);}
    s.page=UiPage::Passcode;view.render(s);verifyHits(view,"welcome password controls preserved");
    s.page=UiPage::Input;s.netCredentials=false;s.frozen=true;view.render(s);
    bool management=false;for(int i=0;i<view.hitCount;i++)management |= view.hits[i].action==UiAction::OpenNetwork||view.hits[i].action==UiAction::OpenPasscode;
    check(!management,"frozen request has no management entry");
}
int main(int argc,char **argv) {
    try {
        if (argc != 2) throw std::runtime_error("usage: native_tests OUTPUT_DIR");
        testPolicy(); testRoster(); testNetworkRules(); testView(argv[1]);
        std::cout << "ALL " << passed << " checks passed. Host mocks, NOT hardware verification.\n";
        return 0;
    } catch(const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n'; return 1;
    }
}
