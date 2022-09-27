// hue: using 0 to 1/3 => red to green.
double tmp2hue(double tmp, double hi, double lo)
{
    double hue = 0;
    if (tmp < lo)
        return 1.0 / 3.0;
    else if (tmp > hi)
        return 0.0;
    else
        return (hi - tmp) / (hi - lo) / 3.0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////https://en.wikipedia.org/wiki/HSL_and_HSV#HSV_to_RGB
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double hsv_k(int n, double hue)
{
    return fmod(n + hue / 60.0, 6);
}

double hsv_f(int n, double hue, double s, double v)
{
    double k = hsv_k(n, hue);
    return v - v * s * max({min({k, 4 - k, 1.0}), 0.0});
}

vector<int> hsv2rgb(double h, double s, double v)
{
    double hue = h * 360;
    int r = int(hsv_f(5, hue, s, v) * 255);
    int g = int(hsv_f(3, hue, s, v) * 255);
    int b = int(hsv_f(1, hue, s, v) * 255);
    vector<int> rgb;
    rgb.push_back(r);
    rgb.push_back(g);
    rgb.push_back(b);
    return rgb;
}
