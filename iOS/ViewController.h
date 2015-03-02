#import <UIKit/UIKit.h>

@interface ViewController: UIViewController<UIActionSheetDelegate>

@property (nonatomic, retain) IBOutlet UILabel *mainTitle;
@property (nonatomic, retain) IBOutlet UILabel *model;
@property (nonatomic, retain) IBOutlet UILabel *os;

@property (nonatomic, retain) IBOutlet UIView *info;

@property (nonatomic, retain) IBOutlet UIView *status;
@property (nonatomic, retain) IBOutlet UILabel *statusMessage;
@property (nonatomic, retain) IBOutlet UIPageControl *progress;

@property (nonatomic, retain) IBOutlet UIView *results;
@property (nonatomic, retain) IBOutlet UILabel *latency;
@property (nonatomic, retain) IBOutlet UILabel *bufferSize;
@property (nonatomic, retain) IBOutlet UILabel *sampleRate;
@property (nonatomic, retain) IBOutlet UILabel *network;
@property (nonatomic, retain) IBOutlet UILabel *website;

@property (nonatomic, retain) IBOutlet UILabel *button;
@property (nonatomic, retain) IBOutlet UIImageView *superpowered;

@end
