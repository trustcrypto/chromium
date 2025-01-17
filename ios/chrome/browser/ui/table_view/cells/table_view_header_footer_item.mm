// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewHeaderFooterItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [UITableViewHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  DCHECK([headerFooter class] == self.cellClass);
  headerFooter.accessibilityTraits = self.accessibilityTraits;
  headerFooter.accessibilityIdentifier = self.accessibilityIdentifier;
  // Use the styler tableViewSectionHeaderBlurEffect if available, if not use
  // the styler tableViewBackgroundColor (as a performance optimization) if
  // available.
  if (styler.tableViewSectionHeaderBlurEffect) {
    UIVisualEffectView* visualEffect = [[UIVisualEffectView alloc]
        initWithEffect:styler.tableViewSectionHeaderBlurEffect];
    headerFooter.backgroundView = visualEffect;
  } else if (styler.tableViewBackgroundColor) {
    UIView* backgroundView = [[UIView alloc] init];
    backgroundView.backgroundColor = styler.tableViewBackgroundColor;
    headerFooter.backgroundView = backgroundView;
  }
}

@end
