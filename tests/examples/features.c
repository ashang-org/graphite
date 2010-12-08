
#include <graphite2/Font.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    gr_uint16 i;
    gr_uint16 langId = 0x0409;
    gr_uint32 lang = 0;
    char idtag[5] = {0, 0, 0, 0, 0};                                /*<1>*/

    GrFace *face = gr_make_file_face(argv[1], 0);
    if (!face) return 1;
    if (argc > 2) lang = gr_str_to_tag(argv[2]);
    GrFeatureVal *features = gr_face_featureval_for_lang(face, lang);  /*<2>*/
    if (!features) return 2;
    int num = gr_face_n_fref(face);
    for (i = 0; i < num; ++i)
    {
        const GrFeatureRef *fref = gr_face_fref(face, i);              /*<3>*/
        gr_uint32 length = 0;
        char *label = gr_fref_label(fref, &langId, gr_utf8, &length);  /*<4>*/
        printf("%s ", label);
        gr_label_destroy(label);
        gr_uint32 id = gr_fref_id(fref);                               /*<5>*/
        if (id <= 0x00FFFFFF)
            printf("(%d)\n", id);
        else
        {
            gr_tag_to_str(id, idtag);
            printf("(%s)\n", idtag);
        }
        gr_uint16 val = gr_fref_feature_value(fref, features);
        int numval = gr_fref_n_values(fref);
        int j;

        for (j = 0; j < numval; ++j)
        {
            if (gr_fref_value(fref, j) == val)                         /*<6>*/
            {
                label = gr_fref_value_label(fref, j, &langId, gr_utf8, &length);
                printf("\t%s (%d)\n", label, val);
                gr_label_destroy(label);
            }
        }
    }
    gr_featureval_destroy(features);
    gr_face_destroy(face);
    return 0;
}

