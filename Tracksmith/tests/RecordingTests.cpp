#include "../source/Recorder.h"
#include <iostream>
#include <limits>
#include <cstdlib>
#include <thread>
static void require(bool value, const char* text){if(!value){std::cerr<<"FAIL: "<<text<<'\n';std::exit(1);}}
int main()
{
    {
        RecordQueue queue;std::vector<float> values(RecordQueue::capacity);
        for(size_t i=0;i<values.size();++i)values[i]=static_cast<float>(i)/static_cast<float>(values.size());
        const float* source[]={values.data()};require(queue.push(source,1,static_cast<int>(values.size())),"whole FIFO accepts exact capacity");
        require(!queue.push(source,1,1)&&queue.available()==RecordQueue::capacity,"overflow rejects without corrupting queued audio");
        std::array<const float*,2> data{};
        for(int block=0;block<15;++block){const int n=queue.readable(data);require(n==4096&&data[0][0]==values[static_cast<size_t>(block)*4096],"FIFO order");queue.finishRead(n);}
        require(queue.push(source,1,20),"FIFO wraps");
        int n=queue.readable(data);require(n==4096&&data[0][4095]==values.back(),"old tail survives wrap");queue.finishRead(n);
        n=queue.readable(data);require(n==20&&data[0][19]==values[19],"wrapped head retains samples");queue.finishRead(n);
        require(queue.available()==0&&!queue.push(nullptr,1,20),"empty/invalid input safe");
        float bad[]={std::numeric_limits<float>::quiet_NaN(),2.0f,-2.0f};const float* invalid[]={bad};
        require(queue.push(invalid,1,3),"sanitize input block");n=queue.readable(data);require(n==3&&data[0][0]==0&&data[0][1]==1&&data[0][2]==-1,"nonfinite and out-of-range samples sanitize");queue.finishRead(n);
    }
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("work/recording-tests");require(folder.createDirectory().wasOk(),"recording test folder");
    float left[512],right[512];for(int i=0;i<512;++i){left[i]=0.25f*static_cast<float>(std::sin(i*0.07));right[i]=0.1f;}
    const float* source[]={left,right};
    auto makeFile=[&](const char* prefix){return folder.getChildFile(juce::String(prefix)+juce::Uuid().toString()+".wav");};
    auto check=[&](const juce::File& file,int channels,int64_t frames){juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::FileInputStream(file),true));require(reader!=nullptr,"WAV reopens after finalization");require(reader->sampleRate==48000&&reader->numChannels==static_cast<unsigned int>(channels)&&reader->bitsPerSample==24&&reader->lengthInSamples==frames,"WAV metadata/frame count");return reader;};
    {
        Recorder recorder;const auto file=makeFile("Stereo-");require(recorder.start(file,48000,2,100000).wasOk(),"stereo start");
        for(int block=0;block<16;++block)recorder.capture(source,2,512);
        recorder.requestStop();require(recorder.waitForFinish(10000)&&recorder.isFinished(),"stop drains and finalizes");
        const auto preview=recorder.waveformPreview();require(preview.size()==18,"waveform bins include partial bin");require(static_cast<float>(preview[0])< -0.24f&&static_cast<float>(preview[1])>0.24f,"waveform captures signed sample extrema");
        const auto result=recorder.takeResult();require(result.error.isEmpty()&&result.frames==8192,"all frames reach disk");
        auto reader=check(file,2,8192);float l[512],r[512];float* data[]={l,r};require(reader->read(data,2,0,512),"read captured samples");
        for(int i=0;i<512;++i)require(std::abs(l[i]-left[i])<1e-6f&&std::abs(r[i]-right[i])<1e-6f,"captured stereo samples match input");
        reader.reset();require(recorder.start(file,48000,1,100).failed(),"existing take is never overwritten");
        const auto mono=makeFile("Mono-");require(recorder.start(mono,48000,1,100).wasOk(),"recorder restarts mono");
        require(recorder.waveformPreview().isEmpty(),"preview clears for new take");recorder.capture(source,2,512);require(recorder.waitForFinish(10000),"frame limit stops automatically");
        const auto limited=recorder.takeResult();require(limited.error.isEmpty()&&limited.warning.isNotEmpty()&&limited.frames==100,"limit preserves exact partial take");check(mono,1,100);
    }
    {
        Recorder recorder;const auto file=makeFile("InputLost-");require(recorder.start(file,48000,1,100000).wasOk(),"input-loss test start");
        recorder.capture(source,2,512);recorder.inputStopped();require(recorder.waitForFinish(10000),"input loss finishes writer");
        const auto result=recorder.takeResult();require(result.error.isEmpty()&&result.warning.isNotEmpty()&&result.frames==512,"input loss keeps partial take");check(file,1,512);
    }
    {
        Recorder recorder;const auto file=makeFile("Missing-");require(recorder.start(file,48000,2,1000).wasOk(),"missing-input start");
        recorder.capture(source,1,512);require(recorder.waitForFinish(10000),"missing stereo channel stops");
        require(recorder.takeResult().error.isNotEmpty(),"no frames reports input error");
    }
    {
        Recorder recorder;
        for (int attempt=0;attempt<10;++attempt)
        {
            const auto file=makeFile("ConcurrentStop-");require(recorder.start(file,48000,2,100000).wasOk(),"concurrent-stop start");
            recorder.capture(source,2,512);
            std::atomic<bool> started{false};
            std::thread producer([&]{started=true;for(int block=0;block<64;++block)recorder.capture(source,2,512);});
            while(!started.load())std::this_thread::yield();
            recorder.requestStop();producer.join();require(recorder.waitForFinish(10000),"concurrent stop drains producer");
            const auto result=recorder.takeResult();require(result.error.isEmpty()&&result.frames==static_cast<uint64_t>(std::llround(recorder.elapsed()*48000)),"all committed frames survive concurrent stop");
            check(file,2,static_cast<int64_t>(result.frames));
        }
    }
    {
        Recorder recorder;const auto file=makeFile("SelectedInput-");require(recorder.start(file,48000,1,100000).wasOk(),"selected-input recording start");
        const float* routed[]={nullptr,left,nullptr,right};recorder.capture(routed,4,512,1);recorder.requestStop();require(recorder.waitForFinish(10000),"selected input drains");
        require(recorder.takeResult().error.isEmpty(),"selected input records without error");auto reader=check(file,1,512);
        float captured[512];float* data[]={captured};require(reader->read(data,1,0,512),"selected input WAV reads");
        for(int n=0;n<512;++n)require(std::abs(captured[n]-left[n])<1e-6f,"WAV contains physical input 2 despite disabled input 1");
    }
    std::cout<<"All recording buffer, input selection and WAV writer tests passed.\n";
}

