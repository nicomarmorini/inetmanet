#ifndef __Byte__
#define __Byte__

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include <signal.h>

class Byte {

	private:
	  std::vector<int> theByteArray;
	public:

		Byte() {

		}

		Byte(const Byte& b) {
			for(int i=0;i<b.size();i++) {
				this->theByteArray.push_back(b[i]);
			}
		}

		Byte(std::string s) {
			for(unsigned int i=0;i<s.size();i++) {
				this->theByteArray.push_back((int)s.at(i));
			}
		}

		// 1 byte
		Byte(int num) {
			this->theByteArray.push_back(num);
		}

		Byte(char s) {
			this->theByteArray.push_back((int)s);
		}


		int size() const {
			return(this->theByteArray.size());
		}

		std::vector<int> getByteData() {
			return(this->theByteArray);
		}

		int intValue() {
 			return(this->theByteArray[0]);
		}

		std::string stringValue() {
			std::ostringstream oss;
			for(unsigned int i=0;i<this->theByteArray.size();i++) {
				oss << (char)this->theByteArray[i];
			}
			return(oss.str());
		}

		std::string hexValue() {
			std::ostringstream oss;

			for(unsigned int i=0;i<this->theByteArray.size();i++) {
				oss << std::hex << this->theByteArray[i] << " ";
			}
			return(oss.str());
		}

		void concat(Byte b) {
			for(int i=0;i<b.size();i++)
			{
				this->theByteArray.push_back(b[i]);
			}
		}

		int operator[] (unsigned int pos) const {
			if (pos<this->theByteArray.size()) {
				return(this->theByteArray[pos]);
			}
			return -1;
		}

		Byte& operator=(Byte b){

			if(this == &b) {
				return (*this);
			}
			this->theByteArray.clear();
			for(int i=0;i<b.size();i++) {
				int v = b[i];
				this->theByteArray.push_back(v);
			}
			return (*this);
		}

		bool operator == (int integer) {
			return(*this == Byte(integer));
		}

		bool operator == (Byte b) {

			if ((int)this->theByteArray.size() == b.size()) {
				for(unsigned int i=0;i<this->theByteArray.size();i++) {
					if (this->theByteArray[i] != b[i]) {
						return(false);
					}
				}
				return(true);
			}

			return(false);
		}

		operator int() {
			return this->intValue();
		}

		operator std::string() {
			return this->stringValue();
		}

		friend std::ostream& operator << (std::ostream& os, Byte& obj) {
			os << obj.stringValue();
 			return(os);
		}
};

#endif
