# 算法题

## 4 UNSOLVED 寻找两个正序数组的中位数
给定两个大小分别为 m 和 n 的正序（从小到大）数组 nums1 和
nums2。请你找出并返回这两个正序数组的 中位数 。

算法的时间复杂度应该为 O(log (m+n)) 。

<details><summary>Click to expand</summary>

```cpp
class Solution {
 public:
  int getKthElement(const vector<int>& nums1, const vector<int>& nums2, int k) {
    int m = nums1.size(), n = nums2.size();
    int index1 = 0, index2 = 0;
    while (true) {
      if (index1 == m) return nums2[index2 + k - 1];
      if (index2 == n) return nums1[index1 + k - 1];
      if (k == 1) return std::min(nums1[index1], nums2[index2]);
      int newindex1 = std::min(index1 + k / 2 - 1, m - 1);
      int newindex2 = std::min(index2 + k / 2 - 1, n - 1);
      int pivot1 = nums1.at(newindex1);
      int pivot2 = nums2.at(newindex2);
      if (pivot2 >= pivot1) {
        k -= newindex1 - index1 + 1;
        index1 = newindex1 + 1;
      } else {
        k -= newindex2 - index2 + 1;
        index2 = newindex2 + 1;
      }
    }
  }
  double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
    int totallen = nums1.size() + nums2.size();
    if (totallen % 2 == 1)
      return getKthElement(nums1, nums2, (totallen + 1) / 2);
    else
      return getKthElement(nums1, nums2, totallen / 2) / 2.0 +
             getKthElement(nums1, nums2, totallen / 2 + 1) / 2.0;
  }
};
```

二分搜索版本

```cpp
class Solution {
public:
    double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
        int n=nums1.size();
        int m=nums2.size();
        int index1=0,index2=0;
        int left=-1,right=-1;
        int len=n+m;
        for(int i=0;i<=len/2;i++){
            left=right;
            if(index1<n && (index2>=m||nums1[index1]<nums2[index2])){
                right=nums1[index1++];
            }else{
                right=nums2[index2++];
            }
        }
        if(len & 1){
            return right;
        }else{
            return (left+right)/2.0;
        }
    }
};
```

</details>


## 5 最长回文串

给你一个字符串 s，找到 s 中最长的 回文 子串。

<details>

```c++
class Solution {
public:
    string longestPalindrome(string s) {
        int len=s.size();
        int ans=-1;
        int start=0;
        auto expand_center=[&start,&ans,&len,&s](int left,int right){
            while(left>=0 && right<len && s[left]==s[right]){
                left--;
                right++;
            }
            if(int cur_len=right-left-1;cur_len>ans){
                ans=cur_len;
                start=left+1;
            }
        };
        for(int i=0;i<len;i++){
            expand_center(i,i);
            expand_center(i,i+1);
        }
        return s.substr(start,ans);
    }
};
```

</details>
