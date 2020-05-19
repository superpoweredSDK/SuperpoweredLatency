#import "ViewController.h"
#import <AVFoundation/AVAudioSession.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>
#import <Social/Social.h>
#include "latencyMeasurer.h"
#include <sys/utsname.h>
#include "machines.h"

@implementation ViewController {
    AudioUnit au;
    latencyMeasurer *measurer;
    int samplerate, measurerState;
    CADisplayLink *displayLink;
}

@synthesize mainTitle, model, os, info, status, statusMessage, progress, results, latency, bufferSize, sampleRate, network, button, website, superpowered;

// When the user taps on the main button.
- (IBAction)onButton:(id)sender {
    if (!results.hidden) {
        UIActionSheet *sheet = [[UIActionSheet alloc] initWithTitle:@"Share Results" delegate:self cancelButtonTitle:@"Cancel" destructiveButtonTitle:nil otherButtonTitles:@"Twitter", @"Facebook", @"Email", nil];
        [sheet showInView:self.view];
    } else if (measurer) {
        measurer->toggle();
        button.text = (measurer->state > 0) ? @"Start Latency Test" : @"Cancel";
    };
}

// Encodes safe strings for network transfer.
static NSString *encodedString(NSString *str) {
    return (NSString *)CFBridgingRelease(CFURLCreateStringByAddingPercentEscapes(NULL, (CFStringRef)[[str dataUsingEncoding:NSUTF8StringEncoding] base64EncodedStringWithOptions:0], NULL, (CFStringRef)@"!*'();:@&=+$,/?%#[]", kCFStringEncodingASCII));
}

// Called periodically to update the UI.
- (void)UI_update {
    if ((measurerState != measurer->state) || (measurer->latencyMs < 0)) { // Update the UI if the measurer's state has been changed.
        measurerState = measurer->state;

        // Idle state.
        if (measurerState == 0) {
            info.hidden = NO;
            results.hidden = status.hidden = YES;
            button.text = @"Start Latency Test";

        // Result or error.
        } else if (measurerState > 10) {
            if (measurer->latencyMs == 0) {
                statusMessage.text = @"Dispersion too big, please try again.";
                button.text = @"Restart Test";
            } else {
                info.hidden = status.hidden = website.hidden = YES;
                results.hidden = NO;
                latency.text = [NSString stringWithFormat:@"%i ms", measurer->latencyMs];
                bufferSize.text = [NSString stringWithFormat:@"%i", measurer->buffersize];
                sampleRate.text = [NSString stringWithFormat:@"%i Hz", measurer->samplerate];
                network.text = @"Uploading data...";
                button.text = @"Share Results";

                // Uploading the result to our server.
                NSString *url = [NSString stringWithFormat:@"http://superpowered.com/latencydata/input.php?ms=%i&samplerate=%i&buffersize=%i&model=%@&os=%@&build=0", measurer->latencyMs, measurer->samplerate, measurer->buffersize, encodedString(model.text), encodedString([[UIDevice currentDevice] systemVersion])];
                [NSURLConnection sendAsynchronousRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url] cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:30] queue:[NSOperationQueue mainQueue] completionHandler:^(NSURLResponse *response, NSData *data, NSError *error) {
                    if (error) self.network.text = [error localizedDescription];
                    else if ([data length] == 2) {
                        self.network.text = @"Thank you. We've uploaded the result to the latency benchmarking at:";
                        self.website.hidden = NO;
                    } else self.network.text = @"Network error.";
                }];
            };

        // Measurement starts.
        } else if (measurerState == 1) {
            status.hidden = NO;
            results.hidden = info.hidden = YES;
            statusMessage.text = @"? ms";
            progress.currentPage = 0;
            button.text = @"Cancel";

        // Measurement in progress.
        } else {
            if (measurer->latencyMs < 0) statusMessage.text = @"The environment is too loud!";
            else {
                statusMessage.text = [NSString stringWithFormat:@"%i ms", measurer->latencyMs];
                progress.currentPage = measurerState - 1;
            };
        };
    };
}

// Called periodically for audio input/output.
static OSStatus audioProcessingCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) {
    ViewController *self = (__bridge ViewController *)inRefCon;

    if (self->samplerate > 0) {
        // Get input samples.
        AudioUnitRender(self->au, ioActionFlags, inTimeStamp, 1, inNumberFrames, ioData);

        // Process audio. The reason for a separate input and output process is Android.
        // We are measuring with the same code on both iOS and Android, where the input and output must be processed separately.
        self->measurer->processInput((short int *)ioData->mBuffers[0].mData, self->samplerate, inNumberFrames);
        self->measurer->processOutput((short int *)ioData->mBuffers[0].mData);
    };

    return noErr;
}

// Called when the stream format has been changed. Let's read the current samplerate.
static void streamFormatChangedCallback(void *inRefCon, AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement) {
    if ((inScope == kAudioUnitScope_Output) && (inElement == 0)) {
        // Read the current sample rate.
        UInt32 size = 0;
        AudioUnitGetPropertyInfo(inUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &size, NULL);
        AudioStreamBasicDescription format;
        AudioUnitGetProperty(inUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, &size);

        ViewController *self = (__bridge ViewController *)inRefCon;
        self->samplerate = (int)format.mSampleRate;
    };
}

// Setup everything.
- (void)viewDidLoad {
    [super viewDidLoad];
    mainTitle.text = [NSString stringWithFormat:@"Superpowered Latency Test v%@", [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"]];
    if ([[AVAudioSession sharedInstance] respondsToSelector:@selector(recordPermission)] && [[AVAudioSession sharedInstance] respondsToSelector:@selector(requestRecordPermission:)] && ([[AVAudioSession sharedInstance] recordPermission] != AVAudioSessionRecordPermissionGranted)) {
        [[AVAudioSession sharedInstance] requestRecordPermission:^(BOOL granted) {
            if (granted) [self finishLoading];
        }];
    } else [self finishLoading];
}

- (void)finishLoading {
    measurerState = -1000;
    measurer = new latencyMeasurer();

    // Set up the audio session. Prefer 48 kHz and 64 samples.
    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayAndRecord error:NULL];
    // Default mode adds 30 ms of additional latency for the built-in microphone on iOS devices from the first iPad Air.
    [[AVAudioSession sharedInstance] setMode:AVAudioSessionModeMeasurement error:NULL];
    [[AVAudioSession sharedInstance] setPreferredSampleRate:48000 error:NULL];
    [[AVAudioSession sharedInstance] setPreferredIOBufferDuration:64.0 / 48000.0 error:NULL];
    [[AVAudioSession sharedInstance] setActive:YES error:NULL];
    //[[AVAudioSession sharedInstance] overrideOutputAudioPort:AVAudioSessionPortOverrideSpeaker error:nil];

    // Set up the RemoteIO audio input/output unit.
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (AudioComponentInstanceNew(component, &au) != 0) abort();

    UInt32 value = 1;
    if (AudioUnitSetProperty(au, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &value, sizeof(value))) abort();
    value = 1;
    if (AudioUnitSetProperty(au, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &value, sizeof(value))) abort();

    AudioUnitAddPropertyListener(au, kAudioUnitProperty_StreamFormat, streamFormatChangedCallback, (__bridge void *)self);

    // 16-bit interleaved stereo PCM. While the native audio format is different, conversion does not add any latency (just some CPU).
    AudioStreamBasicDescription format;
    format.mSampleRate = 0;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
    format.mBitsPerChannel = 16;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 4;
    format.mBytesPerPacket = 4;
    format.mChannelsPerFrame = 2;
    if (AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format))) abort();
    if (AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &format, sizeof(format))) abort();

    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = audioProcessingCallback;
    callbackStruct.inputProcRefCon = (__bridge void *)self;
    if (AudioUnitSetProperty(au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callbackStruct, sizeof(callbackStruct))) abort();

    AudioUnitInitialize(au);
    if (AudioOutputUnitStart(au)) abort();

    // Our buttons are not regular buttons, but labels or images. Add tap events here.
    button.userInteractionEnabled = website.userInteractionEnabled = superpowered.userInteractionEnabled = YES;
    UITapGestureRecognizer *gesture = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onButton:)];
    [button addGestureRecognizer:gesture];
    gesture = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onWebsite:)];
    [website addGestureRecognizer:gesture];
    gesture = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onSuperpowered:)];
    [superpowered addGestureRecognizer:gesture];

    // Display system information.
    NSString *deviceModel = nil;
    struct utsname systemInfo;
    uname(&systemInfo);
    const char **m = machines[0];
    while (m[0]) {
        if (strcmp(m[0], systemInfo.machine) == 0) {
            deviceModel = [NSString stringWithCString:m[1] encoding:NSASCIIStringEncoding];
            break;
        };
        m++;
    };
    if (deviceModel == nil) deviceModel = [[UIDevice currentDevice] model];
    model.text = deviceModel;
    os.text = [NSString stringWithFormat:@"iOS %@", [[UIDevice currentDevice] systemVersion]];

    // Start updating the UI periodically.
    displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(UI_update)];
    displayLink.frameInterval = 1;
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];
}

// Social sharing and website hooks. Let's hope this creates a small viral loop for us. :-)
- (void)composeSocial:(NSString *)serviceType {
    dispatch_async(dispatch_get_main_queue(), ^{
        SLComposeViewController *controller = [SLComposeViewController composeViewControllerForServiceType:serviceType];
        if (!controller) return;

        [controller setInitialText:[NSString stringWithFormat:@"I just tested my %@ with the Superpowered Latency Test App: %i ms.", self.model.text, self->measurer->latencyMs]];
        [controller addURL:[NSURL URLWithString:@"http://superpowered.com/latency"]];
        controller.completionHandler = ^(SLComposeViewControllerResult result) {};

        [self presentViewController:controller animated:YES completion:NULL];
    });
}
- (void)actionSheet:(UIActionSheet *)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex {
    switch (buttonIndex) {
        case 0: if ([SLComposeViewController isAvailableForServiceType:SLServiceTypeTwitter]) [self composeSocial:SLServiceTypeTwitter]; break;
        case 1: if ([SLComposeViewController isAvailableForServiceType:SLServiceTypeFacebook]) [self composeSocial:SLServiceTypeFacebook]; break;
        case 2: {
            NSString *body = [NSString stringWithFormat:@"I just tested my %@ for audio latency.\r\n\r\nTotal roundtrip audio latency is %i ms.\r\nBuffer size is %i.\r\nSample rate is %i Hz.\r\n%@\r\n\r\nTest your mobile device's audio latency with the Superpowered Latency Test App for iOS and Android.\r\nhttp://superpowered.com/latency", model.text, measurer->latencyMs, measurer->buffersize, measurer->samplerate, os.text];
            [[UIApplication sharedApplication] openURL:[NSURL URLWithString:[NSString stringWithFormat:@"mailto:?subject=%@&body=%@", [@"Audio Latency Result" stringByAddingPercentEscapesUsingEncoding:NSASCIIStringEncoding], [body stringByAddingPercentEscapesUsingEncoding:NSASCIIStringEncoding]]]];
        }; break;
    };
}
- (IBAction)onWebsite:(id)sender {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"http://superpowered.com/latency"]];
}
- (IBAction)onSuperpowered:(id)sender {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"http://superpowered.com"]];
}

- (void)dealloc {
    [displayLink invalidate];
    displayLink = nil;
    [[AVAudioSession sharedInstance] setActive:NO error:NULL];
    AudioUnitUninitialize(au);
    AudioComponentInstanceDispose(au);
    delete measurer;
}

@end
